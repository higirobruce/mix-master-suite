#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace mm
{
// First-order all-pass: unity magnitude, phase -2*atan(w/wc), -90 degrees at the corner.
template <typename T> class FirstOrderAllPass
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setCorner(cornerHz);
        reset();
    }
    void setCorner(double hz) noexcept
    {
        cornerHz = std::clamp(hz, 0.01, 0.499 * sr);
        const double t = std::tan(std::numbers::pi * cornerHz / sr);
        c = static_cast<T>((t - 1.0) / (t + 1.0));
    }
    T process(T x) noexcept
    {
        const T y = c * x + x1 - c * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = T(0); }

private:
    double sr = 48000.0;
    double cornerHz = 1000.0;
    T c{};
    T x1{}, y1{};
};
} // namespace mm
