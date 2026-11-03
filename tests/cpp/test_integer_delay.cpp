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

TEST_CASE("integer delay block write and read equal the per-sample forms for any block size and delay")
{
    unsigned seed = 3;
    auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return static_cast<float>(seed >> 8) / 16777216.0f - 0.5f; };
    for (int delay : {0, 1, 17, 64, 200})
    {
        mm::IntegerDelay<float> perSample, block;
        perSample.prepare(200, 2, 128);
        block.prepare(200, 2, 128);
        perSample.setDelay(delay);
        block.setDelay(delay);
        std::vector<float> in(128), outA(128), outB(128);
        const int sizes[] = {1, 128, 7, 64, 128, 3, 100, 128, 5};
        for (int size : sizes)
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < size; ++i)
                {
                    in[static_cast<size_t>(i)] = rnd();
                    outA[static_cast<size_t>(i)] = perSample.process(in[static_cast<size_t>(i)], ch);
                }
                block.writeBlock(in.data(), size, ch);
                block.readBlock(outB.data(), size, delay, ch);
                for (int i = 0; i < size; ++i)
                    REQUIRE(outA[static_cast<size_t>(i)] == outB[static_cast<size_t>(i)]);
                // a per-sample read after a block write sees the same ring (delay 0 = the sample just written)
                REQUIRE(block.read(0, ch) == in[static_cast<size_t>(size - 1)]);
            }
    }
}
