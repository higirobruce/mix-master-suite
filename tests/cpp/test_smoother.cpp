#include "core/smoothing/Smoother.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("linear smoother ramps over rampMs at every sample rate")
{
    for (double sr : mm::test::kSampleRates)
    {
        mm::Smoother<float> s;
        s.prepare(sr, 20.0);
        s.snap(0.0f);
        REQUIRE_FALSE(s.isSmoothing());
        s.setTarget(1.0f);
        REQUIRE(s.isSmoothing());
        int steps = 0;
        float prev = -1.0f;
        while (s.isSmoothing())
        {
            const float v = s.next();
            REQUIRE(v >= prev);
            prev = v;
            ++steps;
            REQUIRE(steps < 100000);
        }
        const int expected = static_cast<int>(std::lround(0.02 * sr));
        REQUIRE(std::abs(steps - expected) <= 1);
        REQUIRE(s.current() == Catch::Approx(1.0f).margin(1e-6));
    }
}

TEST_CASE("snap sets value immediately")
{
    mm::Smoother<float> s;
    s.prepare(48000.0, 20.0);
    s.setTarget(1.0f);
    s.snap(0.25f);
    REQUIRE_FALSE(s.isSmoothing());
    REQUIRE(s.current() == 0.25f);
    REQUIRE(s.target() == 0.25f);
    REQUIRE(s.next() == 0.25f);
}

TEST_CASE("skip equals repeated next")
{
    mm::Smoother<float> a, b;
    for (auto* s : {&a, &b})
    {
        s->prepare(48000.0, 20.0);
        s->snap(0.0f);
        s->setTarget(1.0f);
    }
    for (int i = 0; i < 100; ++i)
        a.next();
    b.skip(100);
    REQUIRE(a.current() == Catch::Approx(b.current()).margin(1e-6));
    REQUIRE(a.isSmoothing() == b.isSmoothing());
}
