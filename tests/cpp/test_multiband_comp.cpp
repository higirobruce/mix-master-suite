#include "Params.h" // generated for multiband_comp
#include "plugins/multiband_comp/MultibandCompProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <string>

TEST_CASE("multiband compressor parameter table, zero latency and per-band readouts")
{
    using namespace mm::plugins::multiband_comp;
    REQUIRE(kNumParams == 45);
    REQUIRE(std::string(id::xover_4_hz) == "xover_4_hz");
    MultibandCompProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    REQUIRE(p.numBands() == 4);
    auto set = [&](const char* id, float real) {
        auto* par = p.apvts().getParameter(id);
        par->setValueNotifyingHost(par->convertTo0to1(real));
    };
    // a -6 dBFS 1.2 kHz tone (band 3 of four) against band 3 at -40 / 20:1 and band 1 at 0 dB: band 3 reduces, band 1 does not
    set("b3_threshold_db", -40.0f);
    set("b3_ratio", 20.0f);
    set("b1_threshold_db", 0.0f);
    set("b3_attack_ms", 0.1f);
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    for (int block = 0; block < 400; ++block)
    {
        for (int i = 0; i < 64; ++i)
        {
            const float v = 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1200.0 * (block * 64 + i) / 48000.0));
            buf.setSample(0, i, v);
            buf.setSample(1, i, v);
        }
        p.processBlock(buf, midi);
    }
    REQUIRE(p.bandReductionDb(2) > 10.0f);
    REQUIRE(p.bandReductionDb(0) < 0.1f);
    REQUIRE(p.bandLevelDb(2) > p.bandLevelDb(0) + 20.0f);
    // the crossover edges the editor draws follow the parameters, clamped a third of an octave apart
    set("xover_1_hz", 400.0f); // crossover 2 sits at its 200 Hz minimum: it is held a third of an octave above
    set("xover_2_hz", 200.0f);
    for (int block = 0; block < 40; ++block) // the crossover parameters smooth over 20 ms
        p.processBlock(buf, midi);
    REQUIRE(p.crossoverHz(0) == Catch::Approx(400.0).margin(0.5));
    REQUIRE(p.crossoverHz(1) == Catch::Approx(400.0 * std::pow(2.0, 1.0 / 3.0)).margin(0.5));
}

#include "helpers/preset_checks.h"
TEST_CASE("every multiband_comp factory preset loads and names itself")
{
    MultibandCompProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::multiband_comp::table(), 8);
}
