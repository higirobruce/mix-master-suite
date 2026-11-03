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
