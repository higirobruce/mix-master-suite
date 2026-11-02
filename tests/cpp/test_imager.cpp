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

TEST_CASE("every imager factory preset loads and names itself")
{
    ImagerProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::imager::table(), 1);
}
