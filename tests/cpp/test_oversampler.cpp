#include "core/common/Math.h"
#include "core/oversampling/Oversampler.h"
#include "helpers/Matrix.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <complex>

namespace
{
// render a stereo buffer through up -> fn(sample) -> down, in blocks of 64
template <typename Fn> std::vector<float> renderOs(mm::Oversampler& os, const std::vector<float>& x, Fn&& fn)
{
    std::vector<float> out(x.size());
    juce::AudioBuffer<float> buf(1, 64);
    for (size_t pos = 0; pos < x.size(); pos += 64)
    {
        const int n = static_cast<int>(std::min<size_t>(64, x.size() - pos));
        for (int i = 0; i < n; ++i)
            buf.setSample(0, i, x[pos + static_cast<size_t>(i)]);
        juce::dsp::AudioBlock<float> block(buf.getArrayOfWritePointers(), 1, 0, static_cast<size_t>(n));
        auto up = os.up(block);
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            up.setSample(0, static_cast<int>(i), fn(up.getSample(0, static_cast<int>(i))));
        os.down(block);
        for (int i = 0; i < n; ++i)
            out[pos + static_cast<size_t>(i)] = buf.getSample(0, i);
    }
    return out;
}

double peakDbBelow(const std::vector<float>& y, double sr, double belowHz, size_t start)
{
    double worst = -200.0;
    for (double f = 100.0; f < belowHz - 300.0; f += 50.0)
        worst = std::max(worst, mm::test::levelDbWindowed(y, sr, f, start, y.size()));
    return worst;
}
} // namespace

TEST_CASE("oversampler latency is exact and passthrough is flat")
{
    for (double sr : mm::test::kMatrixRates)
        for (int factor : {1, 2, 4, 8})
        {
            mm::Oversampler os;
            os.prepare(sr, 64, 1, factor);
            REQUIRE(os.factor() == factor);
            REQUIRE(os.oversampledRate() == Catch::Approx(sr * factor));
            const int L = os.latencySamples();
            if (factor == 1)
                REQUIRE(L == 0);
            auto imp = mm::test::impulse(4096);
            auto y = renderOs(os, imp, [](float v) { return v; });
            size_t peakIdx = 0;
            for (size_t i = 0; i < y.size(); ++i)
                if (std::abs(y[i]) > std::abs(y[peakIdx]))
                    peakIdx = i;
            INFO("sr " << sr << " factor " << factor << " latency " << L << " peak at " << peakIdx);
            REQUIRE(static_cast<int>(peakIdx) == L);
            // passthrough: flat within 0.01 dB up to 0.4 fs
            os.reset();
            for (double f : {100.0, 1000.0, 0.2 * sr, 0.4 * sr})
            {
                auto x = mm::test::sine(f, sr, 0.5);
                os.reset();
                auto yy = renderOs(os, x, [](float v) { return v; });
                const size_t s = x.size() / 2;
                const double diff = mm::test::levelDb(yy, sr, f, s, x.size()) - mm::test::levelDb(x, sr, f, s, x.size());
                REQUIRE(std::abs(diff) < (factor == 1 ? 1e-6 : 0.01));
            }
        }
}

TEST_CASE("oversampling suppresses tanh aliasing of a 19 kHz tone: 4x by 30 dB and 8x by 50 dB more than 1x")
{
    // Drive +6 dB (tanh(2)): a hard-clipped square at +18 dB has 9th/11th harmonics that wrap inside even the 4x
    // domain (171/209 kHz -> 21/17 kHz at 48 k) - that is what ADAA is for (Task 13 / M3), not the oversampler.
    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        const double f0 = 19000.0;
        auto x = mm::test::sine(f0, sr, 1.0, mm::dbToGain(6.0f));
        double alias[9] = {};
        for (int factor : {1, 4, 8})
        {
            mm::Oversampler os;
            os.prepare(sr, 64, 1, factor);
            auto y = renderOs(os, x, [](float v) { return std::tanh(v); });
            alias[factor] = peakDbBelow(y, sr, f0, x.size() / 2);
        }
        INFO("sr " << sr << " worst alias dBFS: 1x " << alias[1] << " 4x " << alias[4] << " 8x " << alias[8]);
        REQUIRE(alias[1] > -40.0);
        REQUIRE(alias[4] < alias[1] - 30.0);
        REQUIRE(alias[8] < alias[1] - 50.0);
        REQUIRE(alias[8] < -80.0);
    }
}
