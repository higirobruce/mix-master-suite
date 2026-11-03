#include "Params.h" // generated for soft_clipper
#include "plugins/soft_clipper/SoftClipperProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <string>

TEST_CASE("soft clipper parameter table, latency per oversampling factor and static curve")
{
    using namespace mm::plugins::soft_clipper;
    REQUIRE(kNumParams == 9);
    REQUIRE(std::string(id::true_peak) == "true_peak");
    SoftClipperProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() > 0);
    REQUIRE(p.getLatencySamples() == p.oversamplerLatency(0)); // 4x
    REQUIRE(p.oversamplerLatency(1) > p.oversamplerLatency(0)); // 8x is longer
    auto set = [&](const char* id, float real) {
        auto* par = p.apvts().getParameter(id);
        par->setValueNotifyingHost(par->convertTo0to1(real));
    };
    set("oversample", 1.0f);
    juce::AudioBuffer<float> buf(2, 64);
    buf.clear();
    juce::MidiBuffer midi;
    for (int block = 0; block < 60; ++block) // warm-up for the 8x path's latency, then the 10 ms crossfade
        p.processBlock(buf, midi);
    REQUIRE(p.getLatencySamples() == p.oversamplerLatency(1));
    // the static curve: unity below the knee, the ceiling above it
    set("ceiling_db", -6.0f);
    set("knee_pct", 0.0f);
    REQUIRE(p.staticGainDb(-20.0f) == Catch::Approx(0.0).margin(1e-4));
    REQUIRE(p.staticGainDb(0.0f) == Catch::Approx(-6.0).margin(0.01));
    REQUIRE(p.staticGainDb(6.0f) == Catch::Approx(-12.0).margin(0.01));
}

#include "helpers/preset_checks.h"
TEST_CASE("every soft_clipper factory preset loads and names itself")
{
    SoftClipperProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::soft_clipper::table(), 8);
}
