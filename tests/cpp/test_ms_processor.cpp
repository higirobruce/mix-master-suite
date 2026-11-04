#include "Params.h" // generated for ms_processor
#include "plugins/ms_processor/MsProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <string>

TEST_CASE("ms processor parameter table, zero latency, correlation meter and scope")
{
    using namespace mm::plugins::ms_processor;
    REQUIRE(kNumParams == 28);
    REQUIRE(std::string(id::side_eq2_q) == "side_eq2_q");
    MsProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    // a mono tone reads correlation 1; an anti-phase tone -1
    for (int sign : {1, -1})
    {
        for (int block = 0; block < 1600; ++block) // 2 s per pair: the 300 ms exponential window keeps 19 % of the previous pair after 0.5 s
        {
            for (int i = 0; i < 64; ++i)
            {
                const float v = 0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * (block * 64 + i) / 48000.0));
                buf.setSample(0, i, v);
                buf.setSample(1, i, static_cast<float>(sign) * v);
            }
            p.processBlock(buf, midi);
        }
        REQUIRE(p.currentCorrelation() == Catch::Approx(static_cast<float>(sign)).margin(0.02));
    }
    std::vector<float> l(2048), r(2048);
    const int n = p.pullScope(l.data(), r.data(), 2048);
    REQUIRE(n == 2048);
    REQUIRE(l[2047] == Catch::Approx(-r[2047]).margin(1e-6));
}

TEST_CASE("ms processor static EQ response is the product of the enabled bands per lane")
{
    MsProcessor p;
    p.prepareToPlay(48000.0, 64);
    auto set = [&](const char* id, float real) {
        auto* par = p.apvts().getParameter(id);
        par->setValueNotifyingHost(par->convertTo0to1(real));
    };
    set("mid_eq1_enabled", 1.0f);
    set("mid_eq1_gain_db", 6.0f); // bell at 300 Hz
    REQUIRE(20.0 * std::log10(std::abs(p.staticResponseAt(0, 300.0))) == Catch::Approx(6.0).margin(0.05));
    REQUIRE(std::abs(p.staticResponseAt(1, 300.0)) == Catch::Approx(1.0).margin(1e-6)); // the side lane is untouched
    set("side_eq2_enabled", 1.0f);
    set("side_eq2_gain_db", -3.0f); // high shelf at 6 kHz
    REQUIRE(20.0 * std::log10(std::abs(p.staticResponseAt(1, 18000.0))) == Catch::Approx(-3.0).margin(0.1));
}

#include "helpers/preset_checks.h"
TEST_CASE("every ms_processor factory preset loads and names itself")
{
    MsProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::ms_processor::table(), 8);
}
