#include "Params.h" // generated for gate
#include "plugins/gate/GateProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
void run(GateProcessor& p, float level, int blocks)
{
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 64; ++n)
                buf.setSample(ch, n, level);
        p.processBlock(buf, midi);
    }
}
} // namespace

TEST_CASE("gate table, lookahead latency, open state and gain-reduction readout")
{
    using namespace mm::plugins::gate;
    REQUIRE(kNumParams == 17);
    GateProcessor p;
    REQUIRE(p.getBusCount(true) == 2);
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    REQUIRE(p.maxDspLatencySamples() == 240);
    REQUIRE_FALSE(p.isOpen());
    run(p, 0.5f, 50); // -6 dBFS, threshold -40: open
    REQUIRE(p.isOpen());
    REQUIRE(p.currentGainReductionDb() < 0.5f);
    run(p, 0.0f, 400); // silence: hold 50 ms then release 100 ms -> closed at the range (-80 dB)
    REQUIRE_FALSE(p.isOpen());
    REQUIRE(p.currentGainReductionDb() > 60.0f);
    auto* la = p.apvts().getParameter(id::lookahead_ms);
    la->setValueNotifyingHost(la->convertTo0to1(5.0f));
    run(p, 0.0f, 20); // smoother settles (20 ms)
    REQUIRE(p.getLatencySamples() == 240);
    p.prepareToPlay(96000.0, 64);
    REQUIRE(p.getLatencySamples() == 480);
    REQUIRE(p.maxDspLatencySamples() == 480);
}

#include "helpers/preset_checks.h"
TEST_CASE("every gate factory preset loads and names itself")
{
    GateProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::gate::table(), 8);
}
