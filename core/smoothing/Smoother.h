#pragma once
#include <cmath>

namespace mm
{
enum class SmoothMode
{
    Linear,     // reaches the target exactly after rampMs
    Exponential // one-pole; within 0.1 % of the target after rampMs, settles (|error| < 1e-5) by 2 x rampMs
};

// Parameter smoother. No allocation; safe on the audio thread. State is kept in double so the exponential update
// never stalls at the float ulp; the linear ramp derives each value from the remaining step count (no drift).
template <typename T> class Smoother
{
public:
    void prepare(double sampleRate, double rampMs, SmoothMode m = SmoothMode::Linear) noexcept
    {
        mode = m;
        rampSamples = static_cast<int>(std::lround(rampMs * 1e-3 * sampleRate));
        if (rampSamples < 1)
            rampSamples = 1;
        // exponential: inside 0.1 % of the step after rampSamples (designed to 1/1100 for float headroom)
        coeff = std::exp(-std::log(1100.0) / static_cast<double>(rampSamples));
        snap(static_cast<T>(currentValue));
    }

    void setTarget(T v) noexcept
    {
        const double t = static_cast<double>(v);
        if (t == targetValue)
            return;
        targetValue = t;
        if (mode == SmoothMode::Linear)
        {
            stepsRemaining = rampSamples;
            increment = (targetValue - currentValue) / static_cast<double>(rampSamples);
        }
        else
            smoothing = true;
    }

    void snap(T v) noexcept
    {
        currentValue = targetValue = static_cast<double>(v);
        stepsRemaining = 0;
        increment = 0.0;
        smoothing = false;
    }

    T next() noexcept
    {
        if (mode == SmoothMode::Linear)
        {
            if (stepsRemaining > 0)
            {
                --stepsRemaining;
                currentValue = targetValue - increment * static_cast<double>(stepsRemaining);
            }
        }
        else if (smoothing)
        {
            currentValue = targetValue + (currentValue - targetValue) * coeff;
            settleCheck();
        }
        return static_cast<T>(currentValue);
    }

    void skip(int n) noexcept
    {
        if (n <= 0)
            return;
        if (mode == SmoothMode::Linear)
        {
            if (stepsRemaining == 0)
                return;
            stepsRemaining = n >= stepsRemaining ? 0 : stepsRemaining - n;
            currentValue = targetValue - increment * static_cast<double>(stepsRemaining);
        }
        else if (smoothing)
        {
            currentValue = targetValue + (currentValue - targetValue) * std::pow(coeff, n);
            settleCheck();
        }
    }

    bool isSmoothing() const noexcept { return mode == SmoothMode::Linear ? stepsRemaining > 0 : smoothing; }
    T current() const noexcept { return static_cast<T>(currentValue); }
    T target() const noexcept { return static_cast<T>(targetValue); }

private:
    void settleCheck() noexcept
    {
        const double tol = 1e-5 * (std::abs(targetValue) > 1.0 ? std::abs(targetValue) : 1.0); // -100 dB
        if (std::abs(targetValue - currentValue) < tol)
        {
            currentValue = targetValue;
            smoothing = false;
        }
    }

    SmoothMode mode = SmoothMode::Linear;
    double currentValue = 0.0;
    double targetValue = 0.0;
    double increment = 0.0;
    double coeff = 0.0;
    int rampSamples = 1;
    int stepsRemaining = 0;
    bool smoothing = false;
};
} // namespace mm
