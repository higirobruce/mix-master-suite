#include "core/nonlinear/Waveshapers.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("waveshapers: odd symmetry, bounded, unit slope at zero, antiderivatives consistent")
{
    using namespace mm::shape;
    struct Pair
    {
        float (*fn)(float);
        float (*ad)(float);
        const char* name;
    };
    const Pair pairs[] = {{tanhS, tanhAD1, "tanh"}, {atanS, atanAD1, "atan"}, {hardClip, hardClipAD1, "hardclip"}};
    for (const auto& p : pairs)
    {
        INFO(p.name);
        for (float x = -6.0f; x <= 6.0f; x += 0.05f)
        {
            REQUIRE(p.fn(-x) == Catch::Approx(-p.fn(x)).margin(1e-6));
            REQUIRE(std::abs(p.fn(x)) <= 1.0f + 1e-6f);
            const double h = 1e-3;
            const double numeric = (static_cast<double>(p.ad(x + static_cast<float>(h))) - p.ad(x)) / h;
            REQUIRE(numeric == Catch::Approx(static_cast<double>(p.fn(x + 0.0005f))).margin(2e-3));
        }
        REQUIRE((p.fn(1e-3f) / 1e-3f) == Catch::Approx(1.0f).margin(1e-3));
    }
    // soft clip with knee: bounded, odd, unit slope, antiderivative consistent
    for (float knee : {0.0f, 0.5f, 1.0f})
        for (float x = -3.0f; x <= 3.0f; x += 0.05f)
        {
            REQUIRE(softClip(-x, knee) == Catch::Approx(-softClip(x, knee)).margin(1e-6));
            REQUIRE(std::abs(softClip(x, knee)) <= 1.0f + 1e-6f);
            const double h = 1e-3;
            const double numeric = (static_cast<double>(softClipAD1(x + static_cast<float>(h), knee)) - softClipAD1(x, knee)) / h;
            REQUIRE(numeric == Catch::Approx(static_cast<double>(softClip(x + 0.0005f, knee))).margin(2e-3));
        }
    // asymmetric: asym(x, 0) == tanh, asym(0, bias) == 0 (DC-free at rest)
    REQUIRE(asym(0.7f, 0.0f) == Catch::Approx(tanhS(0.7f)).margin(1e-6));
    REQUIRE(asym(0.0f, 0.5f) == Catch::Approx(0.0f).margin(1e-6));
}
