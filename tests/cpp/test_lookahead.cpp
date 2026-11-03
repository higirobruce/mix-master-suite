#include "core/dynamics/Lookahead.h"
#include "helpers/Matrix.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <deque>

TEST_CASE("lookahead reports its latency and delays the signal by it")
{
    for (double sr : mm::test::kMatrixRates)
        for (double ms : {0.5, 5.0, 10.0})
        {
            mm::Lookahead<float> la;
            la.prepare(sr, 10.0, 2);
            la.setMs(ms);
            const int expected = static_cast<int>(std::lround(ms * 1e-3 * sr));
            REQUIRE(la.latencySamples() == expected);
            for (int n = 0; n < expected + 5; ++n)
            {
                const float y = la.delaySignal(n == 0 ? 1.0f : 0.0f, 0);
                REQUIRE(y == (n == expected ? 1.0f : 0.0f));
            }
        }
}

TEST_CASE("lookahead max envelope rises latency samples before the delayed signal and equals a sliding max")
{
    const double sr = 48000.0;
    mm::Lookahead<float> la;
    la.prepare(sr, 10.0, 1);
    la.setMs(2.0);
    const int L = la.latencySamples();
    REQUIRE(L == 96);
    // impulse: the envelope must be 1 from n=0 (the look-ahead sees it) until the delayed impulse passes
    for (int n = 0; n < 300; ++n)
    {
        const float level = n == 0 ? 1.0f : 0.0f;
        const float env = la.maxEnvelope(level);
        const float delayed = la.delaySignal(level, 0);
        if (n <= L)
            REQUIRE(env == 1.0f);
        else
            REQUIRE(env == 0.0f);
        REQUIRE(delayed == (n == L ? 1.0f : 0.0f));
    }
    // random levels vs reference sliding max over the next L+1 samples (future window)
    la.prepare(sr, 10.0, 1);
    la.setMs(2.0);
    std::vector<float> x(4000);
    unsigned seed = 7;
    for (auto& v : x)
    {
        seed = seed * 1664525u + 1013904223u;
        v = static_cast<float>(seed >> 8) / 16777216.0f;
    }
    for (int n = 0; n < 4000; ++n)
    {
        const float env = la.maxEnvelope(x[static_cast<size_t>(n)]);
        float ref = 0.0f;
        for (int k = std::max(0, n - L); k <= n; ++k)
            ref = std::max(ref, x[static_cast<size_t>(k)]);
        REQUIRE(env == ref);
    }
}

TEST_CASE("lookahead max envelope equals a sliding max for every window, across a window change")
{
    // windows that do and do not divide the run length, including one sample; a window change restarts the
    // history (the max covers only the levels seen since)
    std::vector<float> x(5000);
    unsigned seed = 11;
    for (auto& v : x)
    {
        seed = seed * 1664525u + 1013904223u;
        v = static_cast<float>(seed >> 8) / 16777216.0f - 0.25f; // negative levels too: the max is a plain max
    }
    for (int windowSamples : {1, 2, 7, 48, 100, 144, 480})
    {
        mm::Lookahead<float> la;
        la.prepare(48000.0, 10.0, 1);
        la.setSamples(windowSamples);
        REQUIRE(la.latencySamples() == windowSamples);
        const int change = 2500, newWindow = windowSamples == 7 ? 100 : 7;
        int since = 0, L = windowSamples;
        for (int n = 0; n < 5000; ++n)
        {
            if (n == change)
            {
                la.setSamples(newWindow);
                L = newWindow;
                since = n;
            }
            const float env = la.maxEnvelope(x[static_cast<size_t>(n)]);
            float ref = -1.0f;
            for (int k = std::max(since, n - L); k <= n; ++k)
                ref = std::max(ref, x[static_cast<size_t>(k)]);
            REQUIRE(env == ref);
        }
    }
}
