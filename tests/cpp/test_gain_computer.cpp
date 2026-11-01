#include "core/dynamics/GainComputer.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("gain computer: hard knee, soft knee, infinite ratio, expander")
{
    mm::GainComputer<float> gc;
    gc.set({-20.0, 4.0, 0.0, false, -80.0});
    REQUIRE(gc.gainDb(-10.0f) == Catch::Approx(-7.5f).margin(1e-5));
    REQUIRE(gc.gainDb(-30.0f) == Catch::Approx(0.0f).margin(1e-6));
    REQUIRE(gc.gainDb(-20.0f) == Catch::Approx(0.0f).margin(1e-6));

    gc.set({-20.0, 4.0, 6.0, false, -80.0});
    REQUIRE(gc.gainDb(-20.0f) == Catch::Approx(-0.5625f).margin(1e-5)); // (1/4-1)(0+3)^2/(2*6)
    REQUIRE(gc.gainDb(-24.0f) == Catch::Approx(0.0f).margin(1e-6));     // below the knee
    REQUIRE(gc.gainDb(-10.0f) == Catch::Approx(-7.5f).margin(1e-5));    // above the knee: same as hard
    REQUIRE(gc.gainDb(-17.0f) == Catch::Approx(-0.75f * 36.0f / 12.0f).margin(1e-5)); // knee top edge, continuous

    gc.set({-20.0, 1000.0, 0.0, false, -80.0}); // infinity
    REQUIRE(gc.gainDb(-10.0f) == Catch::Approx(-10.0f).margin(0.02));

    gc.set({-20.0, 2.0, 0.0, true, -30.0}); // expander 2:1, range -30
    REQUIRE(gc.gainDb(-40.0f) == Catch::Approx(-20.0f).margin(1e-5));
    REQUIRE(gc.gainDb(-80.0f) == Catch::Approx(-30.0f).margin(1e-5));
    REQUIRE(gc.gainDb(-10.0f) == Catch::Approx(0.0f).margin(1e-6));
}
