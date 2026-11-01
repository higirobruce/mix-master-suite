#include "core/common/Denormals.h"
#include "core/common/Math.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("fastTanh is within 1e-4 of std::tanh on [-6, 6]")
{
    for (int i = 0; i <= 1200; ++i)
    {
        const float x = -6.0f + 0.01f * static_cast<float>(i);
        REQUIRE(std::abs(mm::fastTanh(x) - std::tanh(x)) < 1e-4f);
    }
    REQUIRE(mm::fastTanh(100.0f) == Catch::Approx(1.0f).margin(1e-6));
    REQUIRE(mm::fastTanh(-100.0f) == Catch::Approx(-1.0f).margin(1e-6));
}

TEST_CASE("ScopedNoDenormals flushes denormals to zero")
{
    volatile float tiny = 1e-40f;
    volatile float half = 0.5f;
    {
        mm::ScopedNoDenormals guard;
        const float r = tiny * half;
        REQUIRE(r == 0.0f);
    }
}

TEST_CASE("freqToOmega and isNearlyEqual")
{
    REQUIRE(mm::freqToOmega(1000.0, 48000.0) == Catch::Approx(2.0 * 3.14159265358979 * 1000.0 / 48000.0));
    REQUIRE(mm::isNearlyEqual(1.0f, 1.0f + 1e-7f));
    REQUIRE_FALSE(mm::isNearlyEqual(1.0f, 1.01f));
}
