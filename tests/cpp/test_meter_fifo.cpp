#include "core/metering/MeterFifo.h"
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

TEST_CASE("meter fifo capacity and order")
{
    mm::MeterFifo<int, 4> f;
    for (int i = 0; i < 4; ++i)
        REQUIRE(f.push(i));
    REQUIRE_FALSE(f.push(99));
    REQUIRE(f.available() == 4);
    int v = -1;
    for (int i = 0; i < 4; ++i)
    {
        REQUIRE(f.pop(v));
        REQUIRE(v == i);
    }
    REQUIRE_FALSE(f.pop(v));
}

TEST_CASE("meter fifo is safe with one producer and one consumer")
{
    mm::MeterFifo<int, 64> f;
    constexpr int kCount = 100000;
    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i)
            while (!f.push(i))
                std::this_thread::yield();
    });
    int expected = 0;
    while (expected < kCount)
    {
        int v;
        if (f.pop(v))
        {
            REQUIRE(v == expected);
            ++expected;
        }
        else
            std::this_thread::yield();
    }
    producer.join();
}
