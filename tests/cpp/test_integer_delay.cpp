#include "core/delay/IntegerDelay.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("integer delay delays by N samples per channel")
{
    mm::IntegerDelay<float> d;
    d.prepare(64, 2);
    d.setDelay(7);
    for (int n = 0; n < 20; ++n)
    {
        const float in0 = n == 0 ? 1.0f : 0.0f;
        const float in1 = n == 3 ? 1.0f : 0.0f;
        const float out0 = d.process(in0, 0);
        const float out1 = d.process(in1, 1);
        REQUIRE(out0 == (n == 7 ? 1.0f : 0.0f));
        REQUIRE(out1 == (n == 10 ? 1.0f : 0.0f));
    }
}

TEST_CASE("integer delay of zero is passthrough and max delay works")
{
    mm::IntegerDelay<float> d;
    d.prepare(64, 1);
    d.setDelay(0);
    REQUIRE(d.process(0.5f, 0) == 0.5f);
    d.reset();
    d.setDelay(64);
    for (int n = 0; n < 64; ++n)
        REQUIRE(d.process(n == 0 ? 1.0f : 0.0f, 0) == 0.0f);
    REQUIRE(d.process(0.0f, 0) == 1.0f);
}
