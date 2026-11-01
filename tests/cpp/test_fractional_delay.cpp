#include "core/delay/FractionalDelayLine.h"
#include "core/delay/TempoSync.h"
#include "helpers/Matrix.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Interp = mm::DelayInterp;

TEST_CASE("integer delays are exact; fractional delays give the right phase lag; HF droop is documented")
{
    const double sr = 48000.0;
    for (Interp interp : {Interp::Lagrange3, Interp::Thiran1})
    {
        mm::FractionalDelayLine<float> d;
        d.prepare(sr, 0.1, 1);
        d.setInterp(interp);
        d.setSlewMs(0.0);
        d.setDelaySamples(10.0f, 0);
        for (int n = 0; n < 40; ++n)
        {
            d.push(n == 0 ? 1.0f : 0.0f, 0);
            const float y = d.read(0);
            if (interp == Interp::Lagrange3)
                REQUIRE(y == Catch::Approx(n == 10 ? 1.0f : 0.0f).margin(1e-6));
        }
        // fractional 10.5 samples on a 1 kHz sine: lag = 10.5 samples
        d.reset();
        d.setDelaySamples(10.5f, 0);
        auto x = mm::test::sine(1000.0, sr, 1.0);
        auto y = x;
        for (size_t i = 0; i < x.size(); ++i)
        {
            d.push(x[i], 0);
            y[i] = d.read(0);
        }
        const double ph = mm::test::phaseDeg(y, x, sr, 1000.0, x.size() / 2, x.size());
        const double lagSamples = -ph / 360.0 * sr / 1000.0;
        REQUIRE(lagSamples == Catch::Approx(10.5).margin(interp == Interp::Lagrange3 ? 0.02 : 0.05));
        // magnitude error below 0.1 dB up to 0.1 fs (Lagrange3, worst case d = 0.5); Thiran is all-pass (flat)
        for (double f : {1000.0, 2400.0, 4800.0})
        {
            auto xs = mm::test::sine(f, sr, 1.0);
            auto ys = xs;
            d.reset();
            for (size_t i = 0; i < xs.size(); ++i)
            {
                d.push(xs[i], 0);
                ys[i] = d.read(0);
            }
            const double diff = mm::test::levelDb(ys, sr, f, xs.size() / 2, xs.size()) - mm::test::levelDb(xs, sr, f, xs.size() / 2, xs.size());
            REQUIRE(std::abs(diff) < (interp == Interp::Thiran1 ? 0.01 : 0.1));
        }
    }
}

TEST_CASE("delay time slew: a jump from 10 ms to 20 ms produces no discontinuity")
{
    const double sr = 48000.0;
    mm::FractionalDelayLine<float> d;
    d.prepare(sr, 0.1, 1);
    d.setSlewMs(20.0);
    d.setDelaySamples(480.0f, 0);
    auto x = mm::test::sine(200.0, sr, 1.0, 0.5);
    float maxInputJump = 0.0f;
    for (size_t i = 1; i < x.size(); ++i)
        maxInputJump = std::max(maxInputJump, std::abs(x[i] - x[i - 1]));
    float prev = 0.0f, maxOutputJump = 0.0f;
    for (size_t i = 0; i < x.size(); ++i)
    {
        if (i == 24000)
            d.setDelaySamples(960.0f, 0);
        d.push(x[i], 0);
        const float y = d.read(0);
        if (i > 1000)
            maxOutputJump = std::max(maxOutputJump, std::abs(y - prev));
        prev = y;
    }
    REQUIRE(maxOutputJump < 3.0f * maxInputJump);
}

TEST_CASE("delay clamps to the prepared maximum and readAt works")
{
    mm::FractionalDelayLine<float> d;
    d.prepare(48000.0, 0.01, 2); // 480 samples
    d.setSlewMs(0.0);
    d.setDelaySamples(5000.0f, 1);
    for (int n = 0; n < 600; ++n)
    {
        d.push(n == 0 ? 1.0f : 0.0f, 1);
        const float y = d.read(1);
        if (n == 480)
            REQUIRE(y == Catch::Approx(1.0f).margin(1e-6));
        REQUIRE(std::isfinite(y));
    }
    d.reset();
    for (int n = 0; n < 50; ++n)
    {
        d.push(n == 0 ? 1.0f : 0.0f, 0);
        if (n == 7)
            REQUIRE(d.readAt(7.0f, 0) == Catch::Approx(1.0f).margin(1e-6));
    }
}

TEST_CASE("tempo sync note lengths")
{
    REQUIRE(mm::TempoSync::noteSeconds(120.0, 4, false, false) == Catch::Approx(0.5));
    REQUIRE(mm::TempoSync::noteSeconds(120.0, 8, true, false) == Catch::Approx(0.375));
    REQUIRE(mm::TempoSync::noteSeconds(120.0, 8, false, true) == Catch::Approx(1.0 / 6.0));
    REQUIRE(mm::TempoSync::noteSeconds(120.0, 1, false, false) == Catch::Approx(2.0));
    REQUIRE(mm::TempoSync::noteMs(90.0, 16, false, false) == Catch::Approx(1000.0 * 60.0 / 90.0 / 4.0));
}
