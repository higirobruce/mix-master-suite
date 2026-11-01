#pragma once
#include <cmath>

namespace mm
{
// Linear ramp smoother (M0). M1 adds an exponential mode. No allocation; safe on the audio thread.
template <typename T> class Smoother
{
public:
    void prepare(double sampleRate, double rampMs) noexcept
    {
        rampSamples = static_cast<int>(std::lround(rampMs * 1e-3 * sampleRate));
        if (rampSamples < 1)
            rampSamples = 1;
        snap(currentValue);
    }

    void setTarget(T v) noexcept
    {
        if (v == targetValue)
            return;
        targetValue = v;
        stepsRemaining = rampSamples;
        increment = (targetValue - currentValue) / static_cast<T>(rampSamples);
    }

    void snap(T v) noexcept
    {
        currentValue = targetValue = v;
        stepsRemaining = 0;
        increment = T(0);
    }

    T next() noexcept
    {
        if (stepsRemaining > 0)
        {
            currentValue += increment;
            if (--stepsRemaining == 0)
                currentValue = targetValue;
        }
        return currentValue;
    }

    void skip(int n) noexcept
    {
        if (n <= 0 || stepsRemaining == 0)
            return;
        if (n >= stepsRemaining)
        {
            currentValue = targetValue;
            stepsRemaining = 0;
        }
        else
        {
            currentValue += increment * static_cast<T>(n);
            stepsRemaining -= n;
        }
    }

    bool isSmoothing() const noexcept { return stepsRemaining > 0; }
    T current() const noexcept { return currentValue; }
    T target() const noexcept { return targetValue; }

private:
    T currentValue{};
    T targetValue{};
    T increment{};
    int rampSamples = 1;
    int stepsRemaining = 0;
};
} // namespace mm
