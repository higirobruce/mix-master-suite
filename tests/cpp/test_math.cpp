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

#include <catch2/catch_approx.hpp>
TEST_CASE("fast dB conversions stay within 1e-4 dB of the exact ones over the dynamics range and are monotonic")
{
    float lastDb = -1000.0f;
    for (int i = 0; i <= 20000; ++i)
    {
        const float db = -130.0f + 160.0f * static_cast<float>(i) / 20000.0f; // -130 .. +30 dB
        const float g = mm::dbToGain(db);
        REQUIRE(std::abs(mm::fastGainToDb(g) - mm::gainToDb(g)) < 1e-4f); // both floor at -120 dB
        REQUIRE(std::abs(20.0f * std::log10(mm::fastDbToGain(db) / g)) < 1e-4f);
        REQUIRE(mm::fastGainToDb(g) >= lastDb);
        lastDb = mm::fastGainToDb(g);
    }
    REQUIRE(mm::fastGainToDb(0.0f) == Catch::Approx(-120.0f));
    REQUIRE(mm::fastGainToDb(1e-9f) == Catch::Approx(-120.0f));
    REQUIRE(mm::fastDbToGain(0.0f) == Catch::Approx(1.0f).margin(1e-6));
}
