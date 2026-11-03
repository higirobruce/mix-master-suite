#include "Params.h" // generated for bus_comp
#include "gui/NeedleMeter.h"
#include "plugins/bus_comp/BusCompProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <string>

TEST_CASE("bus compressor parameter table, stepped values and latency with and without oversampling")
{
    using namespace mm::plugins::bus_comp;
    REQUIRE(kNumParams == 11);
    REQUIRE(std::string(id::release) == "release");
    BusCompProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    REQUIRE(p.ratioValue() == Catch::Approx(4.0));
    REQUIRE(p.attackMs() == Catch::Approx(1.0));
    REQUIRE(p.releaseMs() < 0.0); // Auto
    auto set = [&](const char* id, float real) {
        auto* par = p.apvts().getParameter(id);
        par->setValueNotifyingHost(par->convertTo0to1(real));
    };
    set("attack", 5.0f);
    set("release", 3.0f);
    set("ratio", 2.0f);
    REQUIRE(p.attackMs() == Catch::Approx(30.0));
    REQUIRE(p.releaseMs() == Catch::Approx(1200.0));
    REQUIRE(p.ratioValue() == Catch::Approx(10.0));
    set("oversample", 1.0f);
    juce::AudioBuffer<float> buf(2, 64);
    buf.clear();
    juce::MidiBuffer midi;
    for (int block = 0; block < 40; ++block) // the switch warms the oversampled path up for its latency, fades 10 ms, then re-reports
        p.processBlock(buf, midi);
    REQUIRE(p.getLatencySamples() > 0);
    REQUIRE(p.getLatencySamples() == p.oversamplerLatency());
}

TEST_CASE("needle meter reaches 99 % of a step in 300 ms")
{
    mm::gui::NeedleMeter meter;
    meter.setTarget(-10.0f);
    for (int i = 0; i < 30; ++i)
        meter.advance(0.01);
    REQUIRE(meter.value() == Catch::Approx(-10.0f * 0.99f).margin(0.1f));
    meter.setTarget(0.0f);
    for (int i = 0; i < 30; ++i)
        meter.advance(0.01);
    REQUIRE(std::abs(meter.value()) < 0.2f);
}

#include "helpers/preset_checks.h"
TEST_CASE("every bus_comp factory preset loads and names itself")
{
    BusCompProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::bus_comp::table(), 8);
}
