#pragma once
// Static waveshapers with first antiderivatives (for ADAA). All odd shapes have unit slope at zero and |y| <= 1.
// Antiderivatives are evaluated in double (suffix D): ADAA divides their difference by a small step and would
// otherwise leave a -60 dBFS signal-correlated error on bass in float.
#include <algorithm>
#include <cmath>
#include <numbers>

namespace mm::shape
{
inline float tanhS(float x) noexcept { return std::tanh(x); }
inline float tanhAD1(float x) noexcept
{
    // log(cosh x), written to avoid overflow for large |x|
    const float a = std::abs(x);
    return a > 15.0f ? a - 0.69314718f : std::log(std::cosh(x));
}

inline float atanS(float x) noexcept
{
    constexpr float k = std::numbers::pi_v<float> * 0.5f;
    return std::atan(k * x) / k;
}
inline float atanAD1(float x) noexcept
{
    constexpr float k = std::numbers::pi_v<float> * 0.5f;
    const float kx = k * x;
    return (x * std::atan(kx) - std::log(1.0f + kx * kx) / (2.0f * k)) / k; // (1/k)[x atan(kx) - ln(1 + (kx)^2) / (2k)]
}

inline double tanhAD1D(double x) noexcept
{
    const double a = std::abs(x);
    return a > 20.0 ? a - 0.6931471805599453 : std::log(std::cosh(x));
}
inline double atanAD1D(double x) noexcept
{
    constexpr double k = std::numbers::pi * 0.5;
    const double kx = k * x;
    return (x * std::atan(kx) - std::log(1.0 + kx * kx) / (2.0 * k)) / k;
}

inline float hardClip(float x) noexcept { return std::clamp(x, -1.0f, 1.0f); }
inline float hardClipAD1(float x) noexcept
{
    const float a = std::abs(x);
    return a <= 1.0f ? 0.5f * x * x : a - 0.5f;
}

inline double hardClipAD1D(double x) noexcept
{
    const double a = std::abs(x);
    return a <= 1.0 ? 0.5 * x * x : a - 0.5;
}

// tanh with a bias offset, re-centred so silence stays at zero: even harmonics grow with |bias|.
inline float asym(float x, float bias) noexcept { return std::tanh(x + bias) - std::tanh(bias); }
inline float asymAD1(float x, float bias) noexcept { return tanhAD1(x + bias) - std::tanh(bias) * x; }
inline double asymAD1D(double x, double bias) noexcept { return tanhAD1D(x + bias) - std::tanh(bias) * x; }

// Soft clipper with knee k in [0, 1]: linear up to 1 - k, quadratic knee to 1 + k, flat beyond (k = 0: hard clip).
inline float softClip(float x, float knee) noexcept
{
    const float k = std::clamp(knee, 0.0f, 1.0f);
    const float t = 1.0f - k;
    const float a = std::abs(x);
    float y;
    if (a <= t)
        y = a;
    else if (a <= 1.0f + k && k > 0.0f)
        y = a - (a - t) * (a - t) / (4.0f * k);
    else
        y = 1.0f;
    return x < 0.0f ? -y : y;
}
inline float softClipAD1(float x, float knee) noexcept
{
    const float k = std::clamp(knee, 0.0f, 1.0f);
    const float t = 1.0f - k;
    const float a = std::abs(x);
    if (a <= t)
        return 0.5f * a * a;
    const float fKnee = [&] {
        const float u = std::min(a, 1.0f + k);
        return 0.5f * t * t + 0.5f * (u * u - t * t) - (k > 0.0f ? (u - t) * (u - t) * (u - t) / (12.0f * k) : 0.0f);
    }();
    if (a <= 1.0f + k)
        return fKnee;
    return fKnee + (a - (1.0f + k));
}
} // namespace mm::shape

namespace mm::shape
{
inline double softClipAD1D(double x, double knee) noexcept
{
    const double k = std::clamp(knee, 0.0, 1.0);
    const double t = 1.0 - k;
    const double a = std::abs(x);
    if (a <= t)
        return 0.5 * a * a;
    const double u = std::min(a, 1.0 + k);
    const double fKnee = 0.5 * t * t + 0.5 * (u * u - t * t) - (k > 0.0 ? (u - t) * (u - t) * (u - t) / (12.0 * k) : 0.0);
    return a <= 1.0 + k ? fKnee : fKnee + (a - (1.0 + k));
}
} // namespace mm::shape
