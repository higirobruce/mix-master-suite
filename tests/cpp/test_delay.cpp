#include "Params.h" // generated for delay
#include "helpers/preset_checks.h"
#include "plugins/delay/DelayProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("delay table, zero latency, stereo output from mono input")
{
    using namespace mm::plugins::delay;
    REQUIRE(kNumParams == 16);
    DelayProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    REQUIRE(p.getBusCount(true) == 1);
    REQUIRE(p.currentBpm() == Catch::Approx(120.0)); // no play head: the fallback tempo
}

TEST_CASE("every delay factory preset loads and names itself")
{
    DelayProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::delay::table(), 1);
}
