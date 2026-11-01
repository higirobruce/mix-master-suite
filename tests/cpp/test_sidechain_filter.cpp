#include "core/dynamics/SidechainFilter.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/Transfer.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("sidechain HP 200 Hz: 50 Hz attenuated by 12 dB/oct Butterworth, 2 kHz untouched; LP mirrors it")
{
    const double sr = 48000.0;
    mm::SidechainFilter<float> sc;
    sc.prepare(sr);
    sc.setHighPass(200.0);
    sc.setLowPass(0.0);
    auto lvl = [&](double f) {
        auto x = mm::test::sine(f, sr, 1.0);
        auto y = x;
        sc.reset();
        for (auto& v : y)
            v = sc.process(v);
        return mm::test::levelDb(y, sr, f, y.size() / 2, y.size()) - mm::test::levelDb(x, sr, f, x.size() / 2, x.size());
    };
    REQUIRE(lvl(50.0) == Catch::Approx(-24.2).margin(0.5)); // 2nd-order Butterworth 2 octaves below (no warping down here)
    REQUIRE(lvl(200.0) == Catch::Approx(-3.01).margin(0.1));
    REQUIRE(std::abs(lvl(2000.0)) < 0.05);
    sc.setHighPass(0.0);
    sc.setLowPass(2000.0);
    REQUIRE(std::abs(lvl(200.0)) < 0.05);
    REQUIRE(lvl(2000.0) == Catch::Approx(-3.01).margin(0.1));
    // 8 kHz at 48 k is warped by the bilinear transform: compare with the design's own transfer function
    const double expected8k = 20.0 * std::log10(std::abs(mm::evaluate(mm::rbj::lowPass(sr, 2000.0, 0.70710678), sr, 8000.0)));
    REQUIRE(lvl(8000.0) == Catch::Approx(expected8k).margin(0.1));
    REQUIRE(expected8k < -24.0);
}
