#include "core/filters/OnePole.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("one-pole HP at 10 Hz removes DC and passes 1 kHz at every sample rate")
{
    for (double sr : mm::test::kSampleRates)
    {
        mm::OnePoleHP<float> hp;
        hp.prepare(sr);
        hp.setCutoff(10.0);
        float last = 1.0f;
        for (int n = 0; n < static_cast<int>(sr); ++n)
            last = hp.process(1.0f);
        REQUIRE(std::abs(last) < 1e-5f);

        hp.reset();
        auto x = mm::test::sine(1000.0, sr, 1.0);
        auto y = x;
        for (auto& v : y)
            v = hp.process(v);
        const size_t start = x.size() / 2, end = x.size();
        const double diff = mm::test::levelDb(y, sr, 1000.0, start, end) - mm::test::levelDb(x, sr, 1000.0, start, end);
        REQUIRE(std::abs(diff) < 0.01);
    }
}
