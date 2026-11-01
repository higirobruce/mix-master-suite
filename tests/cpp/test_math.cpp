#include "core/common/Math.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("dbToGain and gainToDb")
{
    REQUIRE(mm::dbToGain(0.0f) == 1.0f);
    REQUIRE(mm::dbToGain(-6.0206f) == Catch::Approx(0.5f).margin(1e-4));
    REQUIRE(mm::gainToDb(1.0f) == Catch::Approx(0.0f).margin(1e-6));
    REQUIRE(mm::gainToDb(0.0f) == -120.0f);
    REQUIRE(mm::gainToDb(0.0f, -200.0f) == -200.0f);
}
