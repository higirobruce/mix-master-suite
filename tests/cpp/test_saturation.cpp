#include "Params.h" // generated for saturation
#include "helpers/preset_checks.h"
#include "plugins/saturation/SaturationProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <iostream>

TEST_CASE("saturation table, oversampler latency, reference drive and harmonic levels")
{
    using namespace mm::plugins::saturation;
    REQUIRE(kNumParams == 8);
    SaturationProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() > 0);
    REQUIRE(p.getLatencySamples() == p.oversamplerLatency());
    for (int c = 0; c < 4; ++c)
        std::cout << "reference drive " << c << ": " << SaturationProcessor::referenceDriveDb(c) << " dB\n";
    for (int c = 0; c < 4; ++c)
    {
        const float ref = SaturationProcessor::referenceDriveDb(c);
        REQUIRE(ref >= 0.0f);
        REQUIRE(ref <= 24.0f);
        const auto h0 = SaturationProcessor::harmonicLevelsDb(0.0f, c, 0.0f);
        REQUIRE(h0[0] == Catch::Approx(-12.0f).margin(1.0f)); // unity small-signal gain at drive 0
        const auto h = SaturationProcessor::harmonicLevelsDb(12.0f, c, 0.0f);
        for (float v : h)
            REQUIRE(std::isfinite(v));
        REQUIRE(h[0] < -12.0f); // saturation compresses the fundamental (drive adds colour, not level)
        REQUIRE(h[2] > h[1]);   // odd-order shaper at zero bias: 3rd above 2nd
    }
}

TEST_CASE("every saturation factory preset loads and names itself")
{
    SaturationProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::saturation::table(), 1);
}
