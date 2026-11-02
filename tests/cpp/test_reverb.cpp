#include "Params.h" // generated for reverb
#include "helpers/preset_checks.h"
#include "plugins/reverb/ReverbProcessor.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("reverb table, zero latency, stereo from mono")
{
    using namespace mm::plugins::reverb;
    REQUIRE(kNumParams == 13);
    ReverbProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == 0);
    REQUIRE(p.getBusCount(true) == 1);
}

TEST_CASE("every reverb factory preset loads and names itself")
{
    ReverbProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::reverb::table(), 1);
}
