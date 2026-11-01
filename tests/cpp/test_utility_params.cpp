#include "Params.h" // generated for utility
#include "plugins/utility/UtilityProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("utility generated parameter table")
{
    using namespace mm::plugins::utility;
    REQUIRE(std::string(id::gain_db) == "gain_db");
    REQUIRE(kNumParams == 11);
    REQUIRE(table().find("trim_db")->max == 12.0f);
    REQUIRE(table().find("phase_rotation_deg")->max == 179.0f);
    REQUIRE(table().find("nope") == nullptr);
}

TEST_CASE("utility passes audio at defaults with zero latency")
{
    UtilityProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    juce::AudioBuffer<float> buf(2, 64);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < 64; ++n)
            buf.setSample(ch, n, 0.5f);
    juce::MidiBuffer midi;
    p.processBlock(buf, midi);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < 64; ++n)
            REQUIRE(buf.getSample(ch, n) == 0.5f);
}
