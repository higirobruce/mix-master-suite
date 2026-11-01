#pragma once
// Analytic transfer-function evaluation for tests and displays.
#include "core/filters/Biquad.h"
#include <complex>
#include <numbers>

namespace mm
{
// H(e^{jw}) of a normalised biquad at freqHz.
inline std::complex<double> evaluate(const BiquadCoeffs<double>& c, double sampleRate, double freqHz) noexcept
{
    const double w = 2.0 * std::numbers::pi * freqHz / sampleRate;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    return (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
}
} // namespace mm
