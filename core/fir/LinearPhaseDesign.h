#pragma once
// Linear-phase FIR design by frequency sampling (ADR-0011): the target magnitude is the product of the bands' RBJ
// biquad magnitudes (the same designs the minimum-phase EQ runs), sampled on an L-point grid, made zero-phase,
// inverse-transformed and rotated to the centre; the N = L - 1 odd taps symmetric about (N - 1) / 2 are kept and the
// symmetry is enforced exactly. A Tukey taper over the outer half of the impulse removes the truncation edge: resolved
// content lives near the centre and is untouched (<= 0.015 dB), while a band below the resolution floor smears instead
// of ringing across the spectrum (measured 0.57 -> 0.13 dB above two floors, 0.015 above four). Design time only:
// allocates.
#include "core/filters/BiquadDesign.h"
#include "core/filters/FilterType.h"
#include "core/filters/Transfer.h"
#include "core/fir/Fft.h"
#include <cmath>
#include <numbers>
#include <vector>

namespace mm::fir
{
struct BandSpec
{
    FilterType type;
    double hz, q, gainDb;
};

inline BiquadCoeffs<double> bandCoeffs(const BandSpec& b, double sampleRate) noexcept
{
    switch (b.type)
    {
    case FilterType::Peak: return rbj::peak(sampleRate, b.hz, b.gainDb, b.q);
    case FilterType::LowShelf: return rbj::lowShelf(sampleRate, b.hz, b.gainDb, b.q);
    case FilterType::HighShelf: return rbj::highShelf(sampleRate, b.hz, b.gainDb, b.q);
    case FilterType::LowPass: return rbj::lowPass(sampleRate, b.hz, b.q);
    case FilterType::HighPass: return rbj::highPass(sampleRate, b.hz, b.q);
    case FilterType::Notch: return rbj::notch(sampleRate, b.hz, b.q);
    case FilterType::AllPass: break;
    }
    return rbj::allPass(sampleRate, b.hz, b.q);
}

// The frequency below which a feature is no longer resolved by a design of `length` (the response is guaranteed
// within 0.1 dB of the target only above twice this).
inline double resolutionHz(int length, double sampleRate) noexcept { return 2.0 * sampleRate / static_cast<double>(length); }

// The target is the magnitude product of any biquad sections (RBJ, matched, ...): `length` is rounded up to a power
// of two L; the result has L - 1 taps, centre (L - 2) / 2.
inline std::vector<double> design(const std::vector<BiquadCoeffs<double>>& coeffs, int length, double sampleRate)
{
    int L = 2;
    while (L < length)
        L <<= 1;
    std::vector<double> re(static_cast<size_t>(L), 0.0), im(static_cast<size_t>(L), 0.0);
    for (int k = 0; k <= L / 2; ++k)
    {
        const double f = static_cast<double>(k) * sampleRate / static_cast<double>(L);
        double mag = 1.0;
        for (const auto& c : coeffs)
            mag *= std::abs(evaluate(c, sampleRate, f));
        re[static_cast<size_t>(k)] = mag;
        if (k > 0 && k < L / 2)
            re[static_cast<size_t>(L - k)] = mag;
    }
    Fft fft;
    fft.prepare(L);
    fft.inverse(re.data(), im.data()); // real and even: a zero-phase impulse centred at index 0
    const int N = L - 1, centre = (L - 2) / 2;
    std::vector<double> h(static_cast<size_t>(N));
    for (int n = 0; n < N; ++n)
    {
        int src = n - centre;
        if (src < 0)
            src += L;
        h[static_cast<size_t>(n)] = re[static_cast<size_t>(src)];
    }
    for (int i = 0; i < N / 2; ++i)
    {
        const double x = static_cast<double>(centre - i) / static_cast<double>(centre + 1); // 0 at the centre, 1 at the ends
        const double taper = x < 0.5 ? 1.0 : 0.5 * (1.0 + std::cos(2.0 * std::numbers::pi * (x - 0.5)));
        const double v = 0.5 * (h[static_cast<size_t>(i)] + h[static_cast<size_t>(N - 1 - i)]) * taper;
        h[static_cast<size_t>(i)] = v;
        h[static_cast<size_t>(N - 1 - i)] = v;
    }
    return h;
}

inline std::vector<double> design(const std::vector<BandSpec>& bands, int length, double sampleRate)
{
    std::vector<BiquadCoeffs<double>> coeffs;
    coeffs.reserve(bands.size());
    for (const auto& b : bands)
        coeffs.push_back(bandCoeffs(b, sampleRate));
    return design(coeffs, length, sampleRate);
}
} // namespace mm::fir
