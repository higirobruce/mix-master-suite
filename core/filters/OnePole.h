#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace mm
{
// TPT (trapezoidal) one-pole high-pass. Stable under per-sample cutoff changes.
template <typename T> class OnePoleHP
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setCutoff(cutoffHz);
        reset();
    }
    void setCutoff(double hz) noexcept
    {
        cutoffHz = std::clamp(hz, 0.1, 0.49 * sr);
        const double g = std::tan(std::numbers::pi * cutoffHz / sr);
        G = g / (1.0 + g);
    }
    // State is kept in double so the integrator never stalls at the float ulp (DC residual < -200 dB).
    T process(T x) noexcept
    {
        const double v = (static_cast<double>(x) - s) * G;
        const double lp = v + s;
        s = lp + v;
        return static_cast<T>(static_cast<double>(x) - lp);
    }
    void reset() noexcept { s = 0.0; }

private:
    double sr = 48000.0;
    double cutoffHz = 10.0;
    double G = 0.0;
    double s = 0.0;
};
} // namespace mm
