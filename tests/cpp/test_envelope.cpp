#include "core/dynamics/EnvelopeFollower.h"
#include "helpers/Matrix.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
// Samples from 10 % to 90 % of a step in the follower's output (dB domain).
int riseSamples(mm::EnvelopeFollower<float>& env, float fromDb, float toDb, int maxSamples)
{
    int t10 = -1, t90 = -1;
    const float a = fromDb + 0.1f * (toDb - fromDb), b = fromDb + 0.9f * (toDb - fromDb);
    for (int n = 0; n < maxSamples; ++n)
    {
        const float y = env.process(toDb);
        const bool passedA = toDb > fromDb ? y >= a : y <= a;
        const bool passedB = toDb > fromDb ? y >= b : y <= b;
        if (t10 < 0 && passedA)
            t10 = n;
        if (t90 < 0 && passedB)
        {
            t90 = n;
            break;
        }
    }
    return t90 - t10;
}
} // namespace

TEST_CASE("envelope follower attack time (10-90 %) within 10 % of the setting")
{
    for (double sr : mm::test::kMatrixRates)
        for (double attackMs : {0.1, 1.0, 10.0, 100.0})
        {
            mm::EnvelopeFollower<float> env;
            env.prepare(sr);
            env.setAttackMs(attackMs);
            env.setReleaseMs(100.0);
            for (int n = 0; n < 1000; ++n)
                env.process(-60.0f);
            const int rise = riseSamples(env, -60.0f, -6.0f, static_cast<int>(sr));
            const double measuredMs = 1000.0 * rise / sr;
            INFO("sr " << sr << " attack " << attackMs << " measured " << measuredMs);
            REQUIRE(std::abs(measuredMs - attackMs) <= 0.1 * attackMs + 1000.0 / sr); // 10 % + one sample
        }
}

TEST_CASE("envelope follower release time (10-90 %) within 10 % of the setting")
{
    for (double sr : mm::test::kMatrixRates)
        for (double releaseMs : {10.0, 100.0, 1000.0, 2000.0})
        {
            mm::EnvelopeFollower<float> env;
            env.prepare(sr);
            env.setAttackMs(0.1);
            env.setReleaseMs(releaseMs);
            for (int n = 0; n < 1000; ++n)
                env.process(-6.0f);
            const int fall = riseSamples(env, -6.0f, -40.0f, static_cast<int>(10.0 * sr));
            const double measuredMs = 1000.0 * fall / sr;
            INFO("sr " << sr << " release " << releaseMs << " measured " << measuredMs);
            REQUIRE(std::abs(measuredMs - releaseMs) <= 0.1 * releaseMs + 1000.0 / sr);
        }
}

TEST_CASE("auto release is between the fast and slow stage")
{
    mm::EnvelopeFollower<float> env;
    env.prepare(48000.0);
    env.setAttackMs(1.0);
    env.setAutoRelease(true);
    for (int n = 0; n < 48000; ++n)
        env.process(-6.0f);
    const int fall = riseSamples(env, -6.0f, -40.0f, 480000);
    const double ms = 1000.0 * fall / 48000.0;
    REQUIRE(ms > 60.0);
    REQUIRE(ms < 600.0);
}
