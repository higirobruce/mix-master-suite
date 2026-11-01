#include "core/nonlinear/TapeHysteresis.h"
#include "helpers/Matrix.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("tape hysteresis is finite on loud noise at every rate and shows a hysteresis lag")
{
    for (double sr : mm::test::kMatrixRates)
    {
        mm::TapeHysteresis tape;
        tape.prepare(sr);
        tape.set(0.8, 0.8, 0.5);
        unsigned seed = 3;
        for (int n = 0; n < static_cast<int>(10.0 * sr); ++n)
        {
            seed = seed * 1664525u + 1013904223u;
            const float x = 4.0f * ((static_cast<float>(seed >> 8) / 8388608.0f) - 1.0f); // +12 dB noise
            const float y = tape.process(x);
            REQUIRE(std::isfinite(y));
            REQUIRE(std::abs(y) < 10.0f);
        }
        // hysteresis: output lags input on a 100 Hz sine (cross-correlation peak at positive lag)
        tape.reset();
        auto x = mm::test::sine(100.0, sr, 0.5, 0.8);
        auto y = x;
        for (size_t i = 0; i < x.size(); ++i)
            y[i] = tape.process(x[i]);
        const double ph = mm::test::phaseDeg(y, x, sr, 100.0, x.size() / 2, x.size());
        INFO("sr " << sr << " phase " << ph);
        REQUIRE(ph < -0.1);
        REQUIRE(ph > -45.0);
    }
}

TEST_CASE("tape at zero drive, saturation and loop width is nearly linear (THD < 0.5 %)")
{
    // width sets the loop (c = sqrt(1 - width) - 0.01): the coercive term does not scale with drive, so a mid
    // width keeps ~2 % THD at any drive; width 0 is the anhysteretic, linear setting
    const double sr = 96000.0;
    mm::TapeHysteresis tape;
    tape.prepare(sr);
    tape.set(0.0, 0.0, 0.0);
    auto x = mm::test::sine(1000.0, sr, 1.0, 0.5);
    auto y = x;
    for (size_t i = 0; i < x.size(); ++i)
        y[i] = tape.process(x[i]);
    const size_t s = x.size() / 2;
    const double fund = mm::test::levelDbWindowed(y, sr, 1000.0, s, y.size());
    double harm = 0.0;
    for (int h = 2; h <= 9; ++h)
        harm += std::pow(10.0, mm::test::levelDbWindowed(y, sr, 1000.0 * h, s, y.size()) / 10.0);
    const double thd = std::sqrt(harm) / std::pow(10.0, fund / 20.0);
    INFO("THD " << thd * 100.0 << " %"); // measured 0.19 % at -6 dBFS: the model's residual coercivity
    REQUIRE(thd < 0.005);
}
