#include "core/filters/AllPass.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("first-order all-pass: flat magnitude, -90 degrees at the corner")
{
    for (double sr : mm::test::kSampleRates)
    {
        for (double f : {100.0, 1000.0, 10000.0})
        {
            mm::FirstOrderAllPass<float> ap;
            ap.prepare(sr);
            ap.setCorner(1000.0);
            auto x = mm::test::sine(f, sr, 1.0);
            auto y = x;
            for (auto& v : y)
                v = ap.process(v);
            const size_t start = x.size() / 2, end = x.size();
            const double diff = mm::test::levelDb(y, sr, f, start, end) - mm::test::levelDb(x, sr, f, start, end);
            REQUIRE(std::abs(diff) < 0.01);
            if (f == 1000.0)
                REQUIRE(mm::test::phaseDeg(y, x, sr, f, start, end) == Catch::Approx(-90.0).margin(1.0));
        }
    }
}
