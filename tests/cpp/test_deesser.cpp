#include "Params.h" // generated for deesser
#include "helpers/preset_checks.h"
#include "plugins/deesser/DeEsserProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("de-esser table, lookahead latency, sibilance flag and reduction readout")
{
    using namespace mm::plugins::deesser;
    REQUIRE(kNumParams == 9);
    DeEsserProcessor p;
    auto* la = p.apvts().getParameter(id::lookahead_ms);
    la->setValueNotifyingHost(la->convertTo0to1(3.0f));
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 144);
    REQUIRE(p.maxDspLatencySamples() == 144);
    REQUIRE_FALSE(p.sibilantNow());
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    for (int b = 0; b < 100; ++b) // 6 kHz at -6 dBFS: 18 dB over the threshold
    {
        for (int n = 0; n < 64; ++n)
        {
            const float v = 0.5f * std::sin(2.0f * 3.14159265f * 6000.0f * static_cast<float>(b * 64 + n) / 48000.0f);
            buf.setSample(0, n, v);
            buf.setSample(1, n, v);
        }
        p.processBlock(buf, midi);
    }
    REQUIRE(p.sibilantNow());
    REQUIRE(p.currentGainReductionDb() > 10.0f);
}

TEST_CASE("every de-esser factory preset loads and names itself")
{
    DeEsserProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::deesser::table(), 1);
}
