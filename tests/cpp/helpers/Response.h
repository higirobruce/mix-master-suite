#pragma once
// Frequency-response measurement from an impulse response (naive DFT at chosen frequencies: exact, no windowing).
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

namespace mm::test
{
// Log-spaced frequencies between lo and hi (inclusive), n points.
inline std::vector<double> logFrequencies(double lo, double hi, int n)
{
    std::vector<double> f(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        f[static_cast<size_t>(i)] = lo * std::pow(hi / lo, static_cast<double>(i) / (n - 1));
    return f;
}

// H(e^{jw}) of a (finite, decayed) impulse response at freqHz.
inline std::complex<double> responseAt(const std::vector<double>& ir, double sr, double freqHz)
{
    const double w = 2.0 * std::numbers::pi * freqHz / sr;
    std::complex<double> acc = 0.0;
    for (size_t n = 0; n < ir.size(); ++n)
        acc += ir[n] * std::polar(1.0, -w * static_cast<double>(n));
    return acc;
}

inline double magDb(std::complex<double> h) { return 20.0 * std::log10(std::abs(h) + 1e-300); }
inline double phaseDeg(std::complex<double> h) { return std::arg(h) * 180.0 / std::numbers::pi; }

// Smallest signed angular difference in degrees, in (-180, 180].
inline double phaseDiffDeg(double a, double b)
{
    double d = std::fmod(a - b + 540.0, 360.0) - 180.0;
    return d <= -180.0 ? d + 360.0 : d;
}

// Impulse response of a per-sample processor: fn(double) -> double (or float), length samples.
template <typename Fn> std::vector<double> impulseResponse(Fn&& fn, int length)
{
    std::vector<double> ir(static_cast<size_t>(length));
    for (int n = 0; n < length; ++n)
        ir[static_cast<size_t>(n)] = static_cast<double>(fn(n == 0 ? 1.0 : 0.0));
    return ir;
}
} // namespace mm::test
