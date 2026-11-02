#include "Params.h" // generated for imager
#include "helpers/preset_checks.h"
#include "plugins/imager/ImagerProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <random>

TEST_CASE("imager table, zero latency, correlation meter accuracy on known pairs, scope ring")
{
    using namespace mm::plugins::imager;
    REQUIRE(kNumParams == 10);
    ImagerProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    std::mt19937 rng(7);
    std::normal_distribution<float> dist(0.0f, 0.1f);
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    for (float rho : {1.0f, 0.5f, 0.0f, -1.0f})
    {
        for (int b = 0; b < 1600; ++b) // ~2.1 s: the 300 ms exponential window keeps < 0.1 % of the previous pair
        {
            for (int n = 0; n < 64; ++n)
            {
                const float a = dist(rng), c = dist(rng);
                const float l = a;
                const float r = rho * a + std::sqrt(juce::jmax(0.0f, 1.0f - rho * rho)) * c; // correlation rho with l
                buf.setSample(0, n, l);
                buf.setSample(1, n, r);
            }
            p.processBlock(buf, midi);
        }
        REQUIRE(p.currentCorrelation() == Catch::Approx(rho).margin(0.02));
    }
    std::vector<float> l(4096), r(4096);
    REQUIRE(p.pullScope(l.data(), r.data(), 4096) > 0);
}

TEST_CASE("imager mono-below stays engaged across a reset (no 20 ms side fade-in)")
{
    // review finding: resetDsp cleared the engagement, so after a transport reset or rate change the side below
    // the mono frequency passed for 20 ms
    ImagerProcessor p;
    p.prepareToPlay(48000.0, 64);
    auto* param = p.apvts().getParameter("mono_below_hz");
    param->setValueNotifyingHost(param->convertTo0to1(200.0f));
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    double phase = 0.0;
    auto runBlocks = [&](int blocks, auto&& onSample) {
        for (int b = 0; b < blocks; ++b)
        {
            for (int n = 0; n < 64; ++n)
            {
                const float s = 0.25f * std::sin(static_cast<float>(phase));
                phase += 2.0 * juce::MathConstants<double>::pi * 60.0 / 48000.0;
                buf.setSample(0, n, s);
                buf.setSample(1, n, -s); // pure side at 60 Hz
            }
            p.processBlock(buf, midi);
            for (int n = 0; n < 64; ++n)
                onSample(b * 64 + n, buf.getSample(0, n));
        }
    };
    runBlocks(400, [](int, float) {}); // engaged and settled
    p.releaseResources();
    p.prepareToPlay(48000.0, 64);
    double energy = 0.0;
    runBlocks(16, [&](int i, float v) { if (i >= 480) energy += static_cast<double>(v) * v; }); // 10..21 ms after the reset
    const double rms = std::sqrt(energy / (16 * 64 - 480));
    REQUIRE(20.0 * std::log10(rms / (0.25 / std::sqrt(2.0))) < -30.0);
}

TEST_CASE("every imager factory preset loads and names itself")
{
    ImagerProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::imager::table(), 1);
}
