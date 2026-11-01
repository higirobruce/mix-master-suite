#include "core/nonlinear/TubeTransfer.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
double harmonicDb(const std::vector<float>& y, double sr, int h)
{
    return mm::test::levelDbWindowed(y, sr, 1000.0 * h, y.size() / 2, y.size());
}
} // namespace

TEST_CASE("tube: bias 0 is odd-only, bias raises the second harmonic monotonically")
{
    const double sr = 96000.0;
    auto x = mm::test::sine(1000.0, sr, 1.0, 0.5);
    double prev2nd = -200.0;
    for (double bias : {0.0, 0.25, 0.5, 0.75})
    {
        mm::TubeTransfer tube;
        tube.set(1.0, bias);
        auto y = x;
        for (size_t i = 0; i < x.size(); ++i)
            y[i] = tube.process(x[i]);
        const double fund = harmonicDb(y, sr, 1), h2 = harmonicDb(y, sr, 2) - fund, h3 = harmonicDb(y, sr, 3) - fund;
        INFO("bias " << bias << " H2 " << h2 << " H3 " << h3 << " dB rel");
        if (bias == 0.0)
            REQUIRE(h2 < -60.0);
        else
        {
            REQUIRE(h2 > prev2nd);
            if (bias >= 0.5)
                REQUIRE(h2 > -30.0);
        }
        prev2nd = h2;
        REQUIRE(std::isfinite(h3));
        for (float v : y)
            REQUIRE(std::isfinite(v));
    }
}
