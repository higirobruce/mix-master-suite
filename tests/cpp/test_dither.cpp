#include "core/common/Dither.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>

TEST_CASE("dither quantises to the bit depth, adds a TPDF floor near -96.3 dBFS at 16 bits, and is off by default")
{
    mm::Dither off;
    REQUIRE(off.process(0.123456f) == 0.123456f);
    mm::Dither d16;
    d16.setBits(16);
    const float q = 32768.0f;
    double noise = 0.0;
    int n = 0;
    for (int i = 0; i < 48000; ++i)
    {
        const float x = 0.001f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * i / 48000.0)); // -60 dBFS
        const float y = d16.process(x);
        REQUIRE(std::abs(y * q - std::round(y * q)) < 1e-3f);
        const float e = y - x;
        noise += static_cast<double>(e) * e;
        ++n;
    }
    const double floorDb = 10.0 * std::log10(noise / n);
    // 16-bit quantisation noise is LSB^2 / 12 = -101.1 dBFS; TPDF dither triples it: -96.3 dBFS
    REQUIRE(floorDb == Catch::Approx(-101.1 + 4.77).margin(1.0));
    mm::Dither d24;
    d24.setBits(24);
    const float q24 = 8388608.0f;
    const float y = d24.process(0.3f);
    REQUIRE(std::abs(y * q24 - std::round(y * q24)) < 1e-2f);
    REQUIRE(std::abs(y - 0.3f) < 2.0f / q24);
}
