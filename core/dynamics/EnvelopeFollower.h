#pragma once
#include <algorithm>
#include <cmath>

namespace mm
{
// Log-domain envelope follower (input and output in dB). Attack/release are 10 %-90 % times of the step response
// (one-pole: a = exp(-ln 9 / samples)). Auto release blends a 60 ms stage into a 500 ms stage according to how
// long the signal has sat above the envelope (short transients release fast, sustained material slowly).
template <typename T> class EnvelopeFollower
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setAttackMs(attackMs);
        setReleaseMs(releaseMs);
        fastCoeff = coeffFor(kAutoFastMs);
        slowCoeff = coeffFor(kAutoSlowMs);
        autoWindow = std::max(1, static_cast<int>(0.5 * sr));
        reset();
    }
    void setAttackMs(double ms) noexcept
    {
        attackMs = std::max(ms, 0.001);
        attackCoeff = coeffFor(attackMs);
    }
    void setReleaseMs(double ms) noexcept
    {
        releaseMs = std::max(ms, 0.001);
        releaseCoeff = coeffFor(releaseMs);
    }
    void setAutoRelease(bool on) noexcept { autoRelease = on; }

    T process(T levelDb) noexcept
    {
        const double x = static_cast<double>(levelDb);
        if (x > env)
        {
            env = x + (env - x) * attackCoeff;
            aboveSamples = std::min(aboveSamples + 1, autoWindow);
        }
        else
        {
            double a = releaseCoeff;
            if (autoRelease)
            {
                const double w = static_cast<double>(aboveSamples) / static_cast<double>(autoWindow); // 0..1
                a = fastCoeff + (slowCoeff - fastCoeff) * w;
            }
            env = x + (env - x) * a;
            if (aboveSamples > 0)
                --aboveSamples;
        }
        return static_cast<T>(env);
    }
    void reset() noexcept
    {
        env = -120.0;
        aboveSamples = 0;
    }
    T current() const noexcept { return static_cast<T>(env); }
    void snapTo(T value) noexcept
    {
        env = static_cast<double>(value);
        aboveSamples = 0;
    }

private:
    double coeffFor(double ms) const noexcept
    {
        const double samples = std::max(1.0, ms * 1e-3 * sr);
        return std::exp(-std::log(9.0) / samples);
    }
    static constexpr double kAutoFastMs = 60.0, kAutoSlowMs = 500.0;
    double sr = 48000.0, attackMs = 10.0, releaseMs = 100.0;
    double attackCoeff = 0.0, releaseCoeff = 0.0, fastCoeff = 0.0, slowCoeff = 0.0;
    double env = -120.0;
    int aboveSamples = 0;
    int autoWindow = 24000; // 0.5 s at 48 k; set in prepare below
    bool autoRelease = false;

public:
    void setAutoWindowSeconds(double s) noexcept { autoWindow = std::max(1, static_cast<int>(s * sr)); }
};
} // namespace mm
