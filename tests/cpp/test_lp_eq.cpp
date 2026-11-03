#include "Params.h" // generated for lp_eq
#include "plugins/lp_eq/LpEqProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <string>

TEST_CASE("linear-phase eq parameter table and latency per FIR length")
{
    using namespace mm::plugins::lp_eq;
    REQUIRE(kNumParams == 59);
    REQUIRE(std::string(id::b8_ms) == "b8_ms");
    LpEqProcessor p;
    p.prepareToPlay(48000.0, 64);
    // default length 4096: partition floor 512 plus the centre (4096 - 2) / 2
    REQUIRE(p.getLatencySamples() == 512 + 2047);
    REQUIRE(p.resolutionHz() == Catch::Approx(2.0 * 48000.0 / 4096.0));
    auto* len = p.apvts().getParameter("fir_length");
    len->setValueNotifyingHost(1.0f); // 16384
    p.redesignForTests();
    juce::AudioBuffer<float> buf(2, 64);
    buf.clear();
    juce::MidiBuffer midi;
    p.processBlock(buf, midi); // the base re-reports the latency at block start
    REQUIRE(p.getLatencySamples() == 512 + 8191);
    REQUIRE(p.resolutionHz() == Catch::Approx(2.0 * 48000.0 / 16384.0));
    // a 4096-sample host block raises the partition to 4096
    LpEqProcessor q;
    q.prepareToPlay(48000.0, 4096);
    REQUIRE(q.getLatencySamples() == 4096 + 2047);
}

TEST_CASE("linear-phase eq static response is the product of the enabled bands in either phase mode")
{
    LpEqProcessor p;
    p.prepareToPlay(48000.0, 64);
    auto set = [&](const char* id, float real) {
        auto* par = p.apvts().getParameter(id);
        par->setValueNotifyingHost(par->convertTo0to1(real));
    };
    set("b3_gain_db", 6.0f); // bell at 250 Hz, linear by default
    REQUIRE(20.0 * std::log10(std::abs(p.staticResponseAt(250.0))) == Catch::Approx(6.0).margin(0.05));
    set("b5_phase", 0.0f);   // minimum
    set("b5_gain_db", -3.0f); // bell at 1600 Hz: its skirt reaches 250 Hz (-0.08 dB) and band 3's reaches 1600
    REQUIRE(20.0 * std::log10(std::abs(p.staticResponseAt(250.0))) == Catch::Approx(6.0).margin(0.2));
    REQUIRE(20.0 * std::log10(std::abs(p.staticResponseAt(1600.0))) == Catch::Approx(-3.0).margin(0.3));
    set("b3_enabled", 0.0f);
    set("b5_enabled", 0.0f);
    REQUIRE(std::abs(p.staticResponseAt(250.0)) == Catch::Approx(1.0).margin(0.02));
}

#include "helpers/preset_checks.h"
TEST_CASE("every lp_eq factory preset loads and names itself")
{
    LpEqProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::lp_eq::table(), 8);
}
