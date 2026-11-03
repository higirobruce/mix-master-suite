#include "Params.h" // generated for limiter
#include "gui/CeilingHistory.h"
#include "plugins/limiter/LimiterProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <string>

TEST_CASE("limiter parameter table, latency = lookahead + oversampler, readouts after a loud tone")
{
    using namespace mm::plugins::limiter;
    REQUIRE(kNumParams == 10);
    REQUIRE(std::string(id::ceiling_dbtp) == "ceiling_dbtp");
    LimiterProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 96 + p.oversamplerLatency()); // 2 ms default
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    for (int block = 0; block < 800; ++block) // ~1 s of a +6 dBFS 1 kHz tone against the -1 dBTP ceiling
    {
        for (int i = 0; i < 64; ++i)
        {
            const float v = 2.0f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * (block * 64 + i) / 48000.0));
            buf.setSample(0, i, v);
            buf.setSample(1, i, v);
        }
        p.processBlock(buf, midi);
    }
    REQUIRE(p.currentGainReductionDb() > 6.0f);
    REQUIRE(p.outputTruePeakDbtp() <= -1.0f + 0.1f);
    REQUIRE(p.outputTruePeakDbtp() > -1.5f);
    REQUIRE(p.integratedLufs() > -10.0f); // a -1 dBTP sine reads about -4 LUFS
    auto* la = p.apvts().getParameter("lookahead_ms");
    la->setValueNotifyingHost(la->convertTo0to1(10.0f));
    for (int block = 0; block < 20; ++block)
        p.processBlock(buf, midi);
    REQUIRE(p.getLatencySamples() == 480 + p.oversamplerLatency());
    la->setValueNotifyingHost(la->convertTo0to1(0.0f)); // lookahead 0 keeps the window's 0.25 ms floor (ADR-0012)
    for (int block = 0; block < 20; ++block)
        p.processBlock(buf, midi);
    REQUIRE(p.getLatencySamples() == 12 + p.oversamplerLatency());
}

TEST_CASE("ceiling history keeps the latest reduction and over-ticks")
{
    mm::gui::CeilingHistory h;
    h.setSize(400, 200);
    for (int i = 0; i < 300; ++i)
        h.push(i < 150 ? 3.0f : 0.0f, i == 100);
    REQUIRE(h.numPoints() == 300);
    REQUIRE(h.latestReductionDb() == Catch::Approx(0.0f));
    REQUIRE(h.overCount() == 1);
    h.clear();
    REQUIRE(h.numPoints() == 0);
    REQUIRE(h.overCount() == 0);
}

#include "helpers/preset_checks.h"
TEST_CASE("every limiter factory preset loads and names itself")
{
    LimiterProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::limiter::table(), 8);
}
