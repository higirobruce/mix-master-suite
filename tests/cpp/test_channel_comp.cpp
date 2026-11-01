#include "Params.h" // generated for channel_comp
#include "plugins/channel_comp/ChannelCompProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("channel compressor table, latency, sidechain bus and gain-reduction readout")
{
    using namespace mm::plugins::channel_comp;
    REQUIRE(kNumParams == 19);
    ChannelCompProcessor p;
    REQUIRE(p.getBusCount(true) == 2);
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    REQUIRE(p.currentGainReductionDb() == Catch::Approx(0.0f).margin(1e-6));
    auto* th = p.apvts().getParameter(id::threshold_db);
    th->setValueNotifyingHost(th->convertTo0to1(-40.0f));
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    for (int b = 0; b < 200; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 64; ++n)
                buf.setSample(ch, n, 0.5f);
        p.processBlock(buf, midi);
    }
    REQUIRE(p.currentGainReductionDb() > 10.0f);
    mm::MeterFrame frame;
    bool got = false;
    while (p.meterFifo().pop(frame))
        got = true;
    REQUIRE(got);
    REQUIRE(frame.gainReductionDb > 10.0f);
}

#include "helpers/preset_checks.h"
TEST_CASE("every channel_comp factory preset loads and names itself")
{
    ChannelCompProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::channel_comp::table(), 8);
}
