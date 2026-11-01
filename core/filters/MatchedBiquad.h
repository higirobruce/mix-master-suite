#pragma once
// Matched-response ("analog") biquads after M. Vicanek, "Matched Second Order Digital Filters" (2016).
// Poles come from impulse invariance of the analog denominator; the numerator is chosen so |H|^2 equals the analog
// prototype exactly at DC, Nyquist and f0 (low/high-pass follow the paper's closed forms). The result tracks the
// analog curve up to Nyquist where bilinear (RBJ) designs cramp.
#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/FilterType.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace mm
{
// |Ha(j w/w0)| of the RBJ analog prototypes, evaluated at hz for a filter centred on f0.
inline std::complex<double> analogPrototype(FilterType t, double f0, double gainDb, double q, double hz) noexcept
{
    const std::complex<double> s(0.0, hz / f0);
    const double A = std::pow(10.0, gainDb / 40.0);
    const std::complex<double> s2 = s * s;
    switch (t)
    {
    case FilterType::Peak: return (s2 + s * (A / q) + 1.0) / (s2 + s / (A * q) + 1.0);
    case FilterType::LowShelf: return A * (s2 + s * (std::sqrt(A) / q) + A) / (A * s2 + s * (std::sqrt(A) / q) + 1.0);
    case FilterType::HighShelf: return A * (A * s2 + s * (std::sqrt(A) / q) + 1.0) / (s2 + s * (std::sqrt(A) / q) + A);
    case FilterType::LowPass: return 1.0 / (s2 + s / q + 1.0);
    case FilterType::HighPass: return s2 / (s2 + s / q + 1.0);
    case FilterType::Notch: return (s2 + 1.0) / (s2 + s / q + 1.0);
    default: return (s2 - s / q + 1.0) / (s2 + s / q + 1.0);
    }
}

namespace matched
{
namespace detail
{
struct Poles
{
    double a1, a2, A0, A1, A2;
};
// wp: digital pole frequency (rad/sample), zeta: damping
inline Poles poles(double wp, double zeta) noexcept
{
    const double a2 = std::exp(-2.0 * zeta * wp);
    const double a1 = -2.0 * std::exp(-zeta * wp) *
                      (zeta <= 1.0 ? std::cos(wp * std::sqrt(1.0 - zeta * zeta)) : std::cosh(wp * std::sqrt(zeta * zeta - 1.0)));
    const double A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2);
    const double A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2);
    return {a1, a2, A0, A1, -4.0 * a2};
}
inline double mag2(FilterType t, double f0, double g, double q, double hz) noexcept
{
    return std::norm(analogPrototype(t, f0, g, q, hz));
}
inline double clampFreq(double f, double sr) noexcept { return std::clamp(f, 1.0, 0.499 * sr); }

// Shelves and peak: match |H|^2 to the analog prototype at DC, Nyquist and f0. When the scaled pole frequency
// passes Nyquist (shelves far above ~0.45 fs) the match has no real solution; fall back to the bilinear design.
inline BiquadCoeffs<double> bilinearFallback(FilterType t, double sr, double f0, double g, double q) noexcept
{
    switch (t)
    {
    case FilterType::Peak: return rbj::peak(sr, f0, g, q);
    case FilterType::LowShelf: return rbj::lowShelf(sr, f0, g, q);
    default: return rbj::highShelf(sr, f0, g, q);
    }
}

inline BiquadCoeffs<double> threePoint(FilterType t, double sr, double f0, double g, double q, double wp, double zeta) noexcept
{
    if (wp >= 0.98 * std::numbers::pi)
        return bilinearFallback(t, sr, f0, g, q);
    const double w0 = 2.0 * std::numbers::pi * f0 / sr;
    const Poles p = poles(wp, zeta);
    const double phi1 = std::sin(0.5 * w0) * std::sin(0.5 * w0);
    const double phi0 = 1.0 - phi1;
    const double phi2 = 4.0 * phi0 * phi1;
    const double D0 = p.A0 * phi0 + p.A1 * phi1 + p.A2 * phi2; // |D|^2 at w0
    const double B0 = p.A0 * mag2(t, f0, g, q, 0.0);
    const double B1 = p.A1 * mag2(t, f0, g, q, 0.5 * sr);
    const double B2 = (mag2(t, f0, g, q, f0) * D0 - B0 * phi0 - B1 * phi1) / phi2;
    const double W = 0.5 * (std::sqrt(B0) + std::sqrt(B1));
    if (!(W * W + B2 >= 0.0) || !(B0 >= 0.0) || !(B1 >= 0.0))
        return bilinearFallback(t, sr, f0, g, q);
    const double b0 = 0.5 * (W + std::sqrt(W * W + B2));
    if (!(b0 > 0.0) || !std::isfinite(b0))
        return bilinearFallback(t, sr, f0, g, q);
    const double b1 = 0.5 * (std::sqrt(B0) - std::sqrt(B1));
    const double b2 = -B2 / (4.0 * b0);
    return {b0, b1, b2, p.a1, p.a2};
}
} // namespace detail

inline BiquadCoeffs<double> peak(double sr, double f, double gainDb, double q) noexcept
{
    if (std::abs(gainDb) < 1e-9)
        return {};
    f = detail::clampFreq(f, sr);
    q = std::max(q, 1e-3);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f / sr;
    return detail::threePoint(FilterType::Peak, sr, f, gainDb, q, w0, 1.0 / (2.0 * A * q));
}

inline BiquadCoeffs<double> lowShelf(double sr, double f, double gainDb, double q) noexcept
{
    if (std::abs(gainDb) < 1e-9)
        return {};
    f = detail::clampFreq(f, sr);
    q = std::max(q, 1e-3);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f / sr;
    return detail::threePoint(FilterType::LowShelf, sr, f, gainDb, q, w0 / std::sqrt(A), 1.0 / (2.0 * q));
}

inline BiquadCoeffs<double> highShelf(double sr, double f, double gainDb, double q) noexcept
{
    if (std::abs(gainDb) < 1e-9)
        return {};
    f = detail::clampFreq(f, sr);
    q = std::max(q, 1e-3);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f / sr;
    return detail::threePoint(FilterType::HighShelf, sr, f, gainDb, q, w0 * std::sqrt(A), 1.0 / (2.0 * q));
}

inline BiquadCoeffs<double> lowPass(double sr, double f, double q) noexcept
{
    f = detail::clampFreq(f, sr);
    q = std::max(q, 1e-3);
    const double w0 = 2.0 * std::numbers::pi * f / sr;
    const detail::Poles p = detail::poles(w0, 1.0 / (2.0 * q));
    const double phi1 = std::sin(0.5 * w0) * std::sin(0.5 * w0), phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;
    const double R1 = (p.A0 * phi0 + p.A1 * phi1 + p.A2 * phi2) * q * q;
    const double B0 = p.A0;
    const double B1 = std::max(0.0, (R1 - B0 * phi0) / phi1);
    const double b0 = 0.5 * (std::sqrt(B0) + std::sqrt(B1));
    const double b1 = std::sqrt(B0) - b0;
    return {b0, b1, 0.0, p.a1, p.a2};
}

inline BiquadCoeffs<double> highPass(double sr, double f, double q) noexcept
{
    f = detail::clampFreq(f, sr);
    q = std::max(q, 1e-3);
    const double w0 = 2.0 * std::numbers::pi * f / sr;
    const detail::Poles p = detail::poles(w0, 1.0 / (2.0 * q));
    const double phi1 = std::sin(0.5 * w0) * std::sin(0.5 * w0), phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;
    const double b0 = q * std::sqrt(p.A0 * phi0 + p.A1 * phi1 + p.A2 * phi2) / (4.0 * phi1);
    return {b0, -2.0 * b0, b0, p.a1, p.a2};
}
} // namespace matched
} // namespace mm
