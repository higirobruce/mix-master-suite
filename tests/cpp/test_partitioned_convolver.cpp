#include "core/fir/PartitionedConvolver.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

namespace
{
std::vector<double> randomTaps(int n, unsigned seed)
{
    std::vector<double> h(static_cast<size_t>(n));
    const double scale = 1.0 / std::sqrt(static_cast<double>(n));
    for (auto& v : h)
    {
        seed = seed * 1664525u + 1013904223u;
        v = (static_cast<double>(seed >> 8) / 16777216.0 - 0.5) * 2.0 * scale;
    }
    return h;
}

std::vector<float> randomSignal(int n, unsigned seed)
{
    std::vector<float> x(static_cast<size_t>(n));
    for (auto& v : x)
    {
        seed = seed * 1664525u + 1013904223u;
        v = static_cast<float>(seed >> 8) / 16777216.0f - 0.5f;
    }
    return x;
}

double residualDbfs(const std::vector<float>& y, const std::vector<double>& ref)
{
    double peak = 0.0;
    for (size_t i = 0; i < y.size(); ++i)
        peak = std::max(peak, std::abs(static_cast<double>(y[i]) - ref[i]));
    return 20.0 * std::log10(std::max(peak, 1e-30));
}
} // namespace

TEST_CASE("partitioned convolver matches direct convolution under a varying block schedule")
{
    const int length = 24000; // 0.5 s at 48 k keeps the direct reference affordable under sanitizers
    const int schedule[] = {64, 480, 1, 4096, 7, 512, 64, 2000, 3};
    for (int taps : {1023, 4097, 16383})
        for (int maxBlock : {64, 4096})
        {
            const auto h = randomTaps(taps, 7u + static_cast<unsigned>(taps));
            const auto x = randomSignal(length, 11u);
            mm::fir::PartitionedConvolver conv;
            conv.prepare(taps, maxBlock, 1);
            conv.setImpulse(h.data(), taps);
            const int P = conv.partitionSize();
            REQUIRE(P >= 64);
            REQUIRE(P >= maxBlock);
            REQUIRE(conv.latencySamples() == P + (taps - 1) / 2);
            // direct convolution delayed by the partition: ref[n] = sum_k h[k] x[n - P - k]
            std::vector<double> ref(static_cast<size_t>(length), 0.0);
            for (int n = 0; n < length; ++n)
            {
                double acc = 0.0;
                const int m = n - P;
                for (int k = 0; k <= std::min(m, taps - 1); ++k)
                    acc += h[static_cast<size_t>(k)] * static_cast<double>(x[static_cast<size_t>(m - k)]);
                ref[static_cast<size_t>(n)] = acc;
            }
            std::vector<float> y(x);
            int pos = 0, s = 0;
            while (pos < length)
            {
                const int n = std::min({schedule[s % 9], maxBlock, length - pos});
                conv.process(y.data() + pos, n, 0);
                pos += n;
                ++s;
            }
            INFO("taps " << taps << " maxBlock " << maxBlock);
            REQUIRE(residualDbfs(y, ref) < -120.0);
        }
}

TEST_CASE("partitioned convolver output impulse lands at the reported latency")
{
    for (int taps : {1023, 4097})
        for (int maxBlock : {64, 1024})
        {
            std::vector<double> h(static_cast<size_t>(taps), 0.0);
            const int centre = (taps - 1) / 2;
            h[static_cast<size_t>(centre)] = 1.0;
            mm::fir::PartitionedConvolver conv;
            conv.prepare(taps, maxBlock, 2);
            conv.setImpulse(h.data(), taps);
            const int L = conv.latencySamples();
            std::vector<float> y(static_cast<size_t>(L + 2 * maxBlock + 10), 0.0f);
            y[0] = 1.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                std::vector<float> buf(y);
                for (int pos = 0; pos < static_cast<int>(buf.size()); pos += maxBlock)
                    conv.process(buf.data() + pos, std::min(maxBlock, static_cast<int>(buf.size()) - pos), ch);
                for (int n = 0; n < static_cast<int>(buf.size()); ++n)
                    REQUIRE(buf[static_cast<size_t>(n)] == Catch::Approx(n == L ? 1.0 : 0.0).margin(1e-9));
            }
        }
}

TEST_CASE("partitioned convolver is silent after reset")
{
    const auto h = randomTaps(4097, 3u);
    mm::fir::PartitionedConvolver conv;
    conv.prepare(4097, 256, 1);
    conv.setImpulse(h.data(), 4097);
    auto x = randomSignal(8192, 5u);
    for (int pos = 0; pos < 8192; pos += 256)
        conv.process(x.data() + pos, 256, 0);
    conv.reset();
    std::vector<float> z(8192, 0.0f);
    for (int pos = 0; pos < 8192; pos += 256)
        conv.process(z.data() + pos, 256, 0);
    for (float v : z)
        REQUIRE(v == 0.0f);
}

TEST_CASE("partitioned convolver takes per-channel spectra computed off-line and crossfades a swap")
{
    const int taps = 1023, maxBlock = 64;
    mm::fir::PartitionedConvolver conv;
    conv.prepare(taps, maxBlock, 2);
    const int P = conv.partitionSize();
    // channel 0: centred impulse at unity; channel 1: centred impulse at -6 dB -> a stereo pair is scaled per channel
    std::vector<double> h1(static_cast<size_t>(taps), 0.0), h2(h1);
    h1[(taps - 1) / 2] = 1.0;
    h2[(taps - 1) / 2] = 0.5;
    mm::fir::PartitionedConvolver::Spectra s1, s2;
    conv.allocateSpectra(s1);
    conv.allocateSpectra(s2);
    conv.computeSpectra(h1.data(), taps, s1);
    conv.computeSpectra(h2.data(), taps, s2);
    conv.setSpectra(0, &s1, 0);
    conv.setSpectra(1, &s2, 0);
    REQUIRE(conv.latencySamples() == P + (taps - 1) / 2);
    const int L = conv.latencySamples();
    std::vector<float> a(static_cast<size_t>(L + 512), 0.0f), b(a);
    a[0] = 1.0f;
    b[0] = 1.0f;
    for (int pos = 0; pos < static_cast<int>(a.size()); pos += maxBlock)
    {
        const int n = std::min(maxBlock, static_cast<int>(a.size()) - pos);
        conv.process(a.data() + pos, n, 0);
        conv.process(b.data() + pos, n, 1);
    }
    REQUIRE(a[static_cast<size_t>(L)] == Catch::Approx(1.0).margin(1e-9));
    REQUIRE(b[static_cast<size_t>(L)] == Catch::Approx(0.5).margin(1e-9));

    // swap channel 0 to the -6 dB impulse with a 20 ms fade on a DC input: the output glides from 1 to 0.5 with no
    // step larger than the fade's own slope, and sits at 0.5 afterwards
    conv.reset();
    const int fade = 960;
    std::vector<float> dc(static_cast<size_t>(L + 4 * fade), 1.0f);
    int pos = 0;
    for (; pos < L + fade; pos += maxBlock)
        conv.process(dc.data() + pos, maxBlock, 0);
    conv.setSpectra(0, &s2, fade);
    for (; pos < static_cast<int>(dc.size()); pos += maxBlock)
        conv.process(dc.data() + pos, std::min(maxBlock, static_cast<int>(dc.size()) - pos), 0);
    float maxStep = 0.0f;
    for (size_t i = static_cast<size_t>(L + 1); i < dc.size(); ++i)
        maxStep = std::max(maxStep, std::abs(dc[i] - dc[i - 1]));
    REQUIRE(maxStep < 2.0f * 0.5f / fade + 1e-6f);
    REQUIRE(dc.back() == Catch::Approx(0.5).margin(1e-6));
}

TEST_CASE("a second swap landing inside a running fade does not restart the mix from the first spectra")
{
    // review finding: setSpectra during a fade reset fadePos to 0 while outFifoPrev still held the first spectra's
    // output, so the mix snapped back by t * (second - first). With a DC input and three centred impulses at 1, 0.5
    // and 0.25 the output must glide monotonically with no step above the fades' own slope.
    const int taps = 255, maxBlock = 64;
    mm::fir::PartitionedConvolver conv;
    conv.prepare(taps, maxBlock, 1);
    std::vector<double> h1(static_cast<size_t>(taps), 0.0), h2(h1), h3(h1);
    h1[(taps - 1) / 2] = 1.0;
    h2[(taps - 1) / 2] = 0.5;
    h3[(taps - 1) / 2] = 0.25;
    mm::fir::PartitionedConvolver::Spectra s1, s2, s3;
    for (auto* s : {&s1, &s2, &s3})
        conv.allocateSpectra(*s);
    conv.computeSpectra(h1.data(), taps, s1);
    conv.computeSpectra(h2.data(), taps, s2);
    conv.computeSpectra(h3.data(), taps, s3);
    conv.setSpectra(0, &s1, 0);
    const int L = conv.latencySamples(), fade = 512;
    std::vector<float> dc(static_cast<size_t>(L + 8 * fade), 1.0f);
    int pos = 0;
    for (; pos < L + fade; pos += maxBlock)
        conv.process(dc.data() + pos, maxBlock, 0);
    conv.setSpectra(0, &s2, fade);
    for (int k = 0; k < 3; ++k, pos += maxBlock) // part-way into the fade (after the partition boundary)
        conv.process(dc.data() + pos, maxBlock, 0);
    conv.setSpectra(0, &s3, fade);
    for (; pos < static_cast<int>(dc.size()); pos += maxBlock)
        conv.process(dc.data() + pos, std::min(maxBlock, static_cast<int>(dc.size()) - pos), 0);
    float maxStep = 0.0f, maxRise = 0.0f;
    for (size_t i = static_cast<size_t>(L + 1); i < dc.size(); ++i)
    {
        maxStep = std::max(maxStep, std::abs(dc[i] - dc[i - 1]));
        maxRise = std::max(maxRise, dc[i] - dc[i - 1]);
    }
    REQUIRE(maxStep < 2.0f * 0.5f / fade + 1e-6f);
    REQUIRE(maxRise < 1e-6f); // never back towards the first spectra
    REQUIRE(dc.back() == Catch::Approx(0.25).margin(1e-6));
}
