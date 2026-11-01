#include "Params.h" // generated for channel_eq
#include "plugins/channel_eq/ChannelEqProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("channel eq parameter table and latency")
{
    using namespace mm::plugins::channel_eq;
    REQUIRE(kNumParams == 36);
    REQUIRE(std::string(id::b3_gain_db) == "b3_gain_db");
    ChannelEqProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
}

TEST_CASE("channel eq evaluateAt is the product of its active sections")
{
    ChannelEqProcessor p;
    p.prepareToPlay(48000.0, 64);
    auto* gain = p.apvts().getParameter("b3_gain_db");
    gain->setValueNotifyingHost(gain->convertTo0to1(6.0f));
    p.updateDesignForTests();
    const auto h = p.evaluateAt(1000.0);
    REQUIRE(20.0 * std::log10(std::abs(h)) == Catch::Approx(6.0).margin(0.05));
    REQUIRE(std::abs(p.evaluateAt(20.0)) == Catch::Approx(1.0).margin(0.01));
}

#include "helpers/preset_checks.h"
TEST_CASE("every channel_eq factory preset loads and names itself")
{
    ChannelEqProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::channel_eq::table(), 8);
}
