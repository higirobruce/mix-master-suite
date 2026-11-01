#pragma once
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

namespace mm::test
{
inline constexpr double kSampleRates[] = {44100.0, 48000.0, 96000.0, 192000.0};

inline std::vector<float> sine(double freqHz, double sr, double seconds, double amplitude = 1.0)
{
    std::vector<float> v(static_cast<size_t>(seconds * sr));
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * freqHz * static_cast<double>(i) / sr));
    return v;
}

inline std::vector<float> impulse(size_t length)
{
    std::vector<float> v(length, 0.0f);
    v[0] = 1.0f;
    return v;
}

// Goertzel: complex amplitude of a single frequency over [start, end).
inline std::complex<double> goertzel(const std::vector<float>& x, double sr, double freqHz, size_t start, size_t end)
{
    const double w = 2.0 * std::numbers::pi * freqHz / sr;
    const double coeff = 2.0 * std::cos(w);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (size_t n = start; n < end; ++n)
    {
        s0 = x[n] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const std::complex<double> y = s1 - std::polar(1.0, -w) * s2;
    return y * 2.0 / static_cast<double>(end - start);
}

inline double levelDb(const std::vector<float>& x, double sr, double freqHz, size_t start, size_t end)
{
    return 20.0 * std::log10(std::abs(goertzel(x, sr, freqHz, start, end)));
}

// Phase of y relative to x at freqHz, degrees in (-180, 180].
inline double phaseDeg(const std::vector<float>& y, const std::vector<float>& x, double sr, double freqHz, size_t start,
                       size_t end)
{
    const auto a = goertzel(y, sr, freqHz, start, end);
    const auto b = goertzel(x, sr, freqHz, start, end);
    return std::arg(a / b) * 180.0 / std::numbers::pi;
}
} // namespace mm::test
