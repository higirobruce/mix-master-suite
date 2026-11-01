#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace mm
{
// First-order all-pass: unity magnitude, phase -2 atan(w/wc), -90 degrees at the corner.
// H(z) = (c + z^-1) / (1 + c z^-1), c = (tan(pi fc/sr) - 1) / (tan(pi fc/sr) + 1).
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
    std::complex<double> evaluate(double hz) const noexcept
    {
        const std::complex<double> z = std::polar(1.0, -2.0 * std::numbers::pi * hz / sr);
        const double cd = static_cast<double>(c);
        return (cd + z) / (1.0 + cd * z);
    }

private:
    double sr = 48000.0;
    double cornerHz = 1000.0;
    T c{};
    T x1{}, y1{};
};

// Second-order all-pass (RBJ): unity magnitude, -180 degrees at the centre, width set by Q. TDF-II, double coeffs.
template <typename T> class SecondOrderAllPass
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setParams(freq, Q);
        reset();
    }
    void setParams(double hz, double q) noexcept
    {
        freq = std::clamp(hz, 1.0, 0.499 * sr);
        Q = std::max(q, 1e-3);
        const double w0 = 2.0 * std::numbers::pi * freq / sr;
        const double alpha = std::sin(w0) / (2.0 * Q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - alpha) / a0;
        b1 = -2.0 * std::cos(w0) / a0;
        b2 = 1.0; // (1 + alpha) / a0
        a1 = b1;
        a2 = b0;
    }
    T process(T x) noexcept
    {
        const double xd = static_cast<double>(x);
        const double y = b0 * xd + s1;
        s1 = b1 * xd - a1 * y + s2;
        s2 = b2 * xd - a2 * y;
        return static_cast<T>(y);
    }
    void reset() noexcept { s1 = s2 = 0.0; }
    std::complex<double> evaluate(double hz) const noexcept
    {
        const std::complex<double> z = std::polar(1.0, -2.0 * std::numbers::pi * hz / sr);
        return (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z);
    }

private:
    double sr = 48000.0, freq = 1000.0, Q = 0.7071;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1 = 0.0, s2 = 0.0;
};
} // namespace mm
