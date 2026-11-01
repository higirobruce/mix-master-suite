#pragma once
// FIR design helpers (double precision, design time only: these allocate).
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace mm::fir
{
enum class Window
{
    Hann,
    Hamming,
    Blackman,
    Kaiser
};

namespace detail
{
inline double besselI0(double x) noexcept
{
    // series expansion, converges quickly for the beta range used here
    double sum = 1.0, term = 1.0;
    const double q = 0.25 * x * x;
    for (int k = 1; k < 200; ++k)
    {
        term *= q / (static_cast<double>(k) * k);
        sum += term;
        if (term < 1e-18 * sum)
            break;
    }
    return sum;
}

inline double windowValue(Window w, int n, int N, double beta) noexcept
{
    const double x = static_cast<double>(n) / static_cast<double>(N - 1); // 0..1 (symmetric)
    switch (w)
    {
    case Window::Hann: return 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * x);
    case Window::Hamming: return 0.54 - 0.46 * std::cos(2.0 * std::numbers::pi * x);
    case Window::Blackman: return 0.42 - 0.5 * std::cos(2.0 * std::numbers::pi * x) + 0.08 * std::cos(4.0 * std::numbers::pi * x);
    case Window::Kaiser:
    {
        const double r = 2.0 * x - 1.0;
        return besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
    }
    }
    return 1.0;
}

inline double sinc(double x) noexcept { return x == 0.0 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x); }
} // namespace detail

// Low-pass windowed sinc. taps odd (rounded up if even), cutoffNorm in cycles/sample (0..0.5), unity DC gain.
inline std::vector<double> windowedSinc(int taps, double cutoffNorm, Window w, double kaiserBeta = 8.6)
{
    if (taps < 3)
        taps = 3;
    if (taps % 2 == 0)
        ++taps;
    cutoffNorm = std::clamp(cutoffNorm, 1e-4, 0.4999);
    std::vector<double> h(static_cast<size_t>(taps));
    const int M = (taps - 1) / 2;
    double sum = 0.0;
    for (int n = 0; n < taps; ++n)
    {
        const double v = 2.0 * cutoffNorm * detail::sinc(2.0 * cutoffNorm * (n - M)) * detail::windowValue(w, n, taps, kaiserBeta);
        h[static_cast<size_t>(n)] = v;
        sum += v;
    }
    for (auto& v : h)
        v /= sum;
    // enforce exact symmetry
    for (int n = 0; n < M; ++n)
        h[static_cast<size_t>(taps - 1 - n)] = h[static_cast<size_t>(n)];
    return h;
}

// Half-band low-pass (cutoff 0.25): Kaiser-windowed sinc with beta chosen from the transition width; every even
// offset from the centre is exactly zero, the centre tap is exactly 0.5.
inline std::vector<double> halfBand(int taps, double transitionNorm)
{
    if (taps < 7)
        taps = 7;
    if (taps % 2 == 0)
        ++taps;
    transitionNorm = std::clamp(transitionNorm, 1e-3, 0.25);
    const double deltaOmega = 2.0 * std::numbers::pi * transitionNorm;
    const double A = 2.285 * deltaOmega * static_cast<double>(taps - 1) + 8.0; // Kaiser estimate
    const double beta = A > 50.0 ? 0.1102 * (A - 8.7) : (A >= 21.0 ? 0.5842 * std::pow(A - 21.0, 0.4) + 0.07886 * (A - 21.0) : 0.0);
    std::vector<double> h(static_cast<size_t>(taps), 0.0);
    const int M = (taps - 1) / 2;
    for (int n = 0; n < taps; ++n)
    {
        const int k = n - M;
        if (k == 0)
            h[static_cast<size_t>(n)] = 0.5;
        else if (k % 2 != 0)
            h[static_cast<size_t>(n)] = 0.5 * detail::sinc(0.5 * k) * detail::windowValue(Window::Kaiser, n, taps, beta);
    }
    double sum = 0.0;
    for (double v : h)
        sum += v;
    for (int n = 0; n < taps; ++n)
        if ((n - M) % 2 != 0)
            h[static_cast<size_t>(n)] *= (1.0 - 0.5) / (sum - 0.5); // normalise the odd taps so DC = 1, centre stays 0.5
    return h;
}
} // namespace mm::fir
