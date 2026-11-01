#pragma once
// RBJ "Audio EQ Cookbook" designs. Shelf slope is expressed through Q (S = 1 at Q = 1/sqrt(2) ... Q is used directly
// as in the cookbook's alpha = sin(w0) / (2 Q) form, so a Q of 0.707 gives the classic maximally flat shelf).
#include "core/filters/Biquad.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace mm::rbj
{
namespace detail
{
inline double clampFreq(double f, double sr) noexcept { return std::clamp(f, 1.0, 0.499 * sr); }
inline BiquadCoeffs<double> normalise(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
{
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
} // namespace detail

inline BiquadCoeffs<double> peak(double sr, double f, double gainDb, double q) noexcept
{
    if (std::abs(gainDb) < 1e-9)
        return {}; // exact identity at 0 dB: a flat band nulls bit-exactly in float too
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * detail::clampFreq(f, sr) / sr;
    const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
    const double cw = std::cos(w0);
    return detail::normalise(1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A);
}

inline BiquadCoeffs<double> lowShelf(double sr, double f, double gainDb, double q) noexcept
{
    if (std::abs(gainDb) < 1e-9)
        return {}; // exact identity at 0 dB: a flat band nulls bit-exactly in float too
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * detail::clampFreq(f, sr) / sr;
    const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
    const double cw = std::cos(w0), sa = 2.0 * std::sqrt(A) * alpha;
    return detail::normalise(A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw),
                             A * ((A + 1) - (A - 1) * cw - sa), (A + 1) + (A - 1) * cw + sa,
                             -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa);
}

inline BiquadCoeffs<double> highShelf(double sr, double f, double gainDb, double q) noexcept
{
    if (std::abs(gainDb) < 1e-9)
        return {}; // exact identity at 0 dB: a flat band nulls bit-exactly in float too
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * detail::clampFreq(f, sr) / sr;
    const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
    const double cw = std::cos(w0), sa = 2.0 * std::sqrt(A) * alpha;
    return detail::normalise(A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw),
                             A * ((A + 1) + (A - 1) * cw - sa), (A + 1) - (A - 1) * cw + sa,
                             2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa);
}

inline BiquadCoeffs<double> lowPass(double sr, double f, double q) noexcept
{
    const double w0 = 2.0 * std::numbers::pi * detail::clampFreq(f, sr) / sr;
    const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
    const double cw = std::cos(w0);
    return detail::normalise((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
}

inline BiquadCoeffs<double> highPass(double sr, double f, double q) noexcept
{
    const double w0 = 2.0 * std::numbers::pi * detail::clampFreq(f, sr) / sr;
    const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
    const double cw = std::cos(w0);
    return detail::normalise((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
}

inline BiquadCoeffs<double> notch(double sr, double f, double q) noexcept
{
    const double w0 = 2.0 * std::numbers::pi * detail::clampFreq(f, sr) / sr;
    const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
    const double cw = std::cos(w0);
    return detail::normalise(1.0, -2 * cw, 1.0, 1 + alpha, -2 * cw, 1 - alpha);
}

inline BiquadCoeffs<double> allPass(double sr, double f, double q) noexcept
{
    const double w0 = 2.0 * std::numbers::pi * detail::clampFreq(f, sr) / sr;
    const double alpha = std::sin(w0) / (2.0 * std::max(q, 1e-3));
    const double cw = std::cos(w0);
    return detail::normalise(1 - alpha, -2 * cw, 1 + alpha, 1 + alpha, -2 * cw, 1 - alpha);
}
} // namespace mm::rbj
