#pragma once
// Topology-preserving (trapezoidal) state-variable filter after Zavalishin / Simper ("Solving the continuous SVF
// equations using trapezoidal integration and equivalent currents", 2013). Stable under per-sample modulation; all
// standard EQ shapes come from one structure through the output mix (m0, m1, m2).
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace mm
{
enum class SvfMode
{
    LowPass,
    HighPass,
    BandPass,
    Notch,
    Peak,
    LowShelf,
    HighShelf,
    AllPass
};

template <typename T> class SVF
{
public:
    using Mode = SvfMode;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        update();
        reset();
    }
    void setMode(Mode m) noexcept
    {
        mode = m;
        update();
    }
    void setParams(double hz, double q, double gainDb) noexcept
    {
        freq = std::clamp(hz, 1.0, 0.499 * sr);
        Q = std::max(q, 1e-3);
        gain = gainDb;
        update();
    }

    T process(T x) noexcept
    {
        const T v3 = x - ic2;
        const T v1 = a1 * ic1 + a2 * v3;
        const T v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = T(2) * v1 - ic1;
        ic2 = T(2) * v2 - ic2;
        return m0 * x + m1 * v1 + m2 * v2;
    }
    void reset() noexcept { ic1 = ic2 = T(0); }

    // Analytic H(e^{jw}): the TPT structure is the bilinear transform of the analog SVF, so S = j tan(w/2) / g.
    std::complex<double> evaluate(double hz) const noexcept
    {
        const std::complex<double> S(0.0, std::tan(std::numbers::pi * hz / sr) / g);
        const std::complex<double> den = S * S + k * S + 1.0;
        return static_cast<double>(m0) + static_cast<double>(m1) * S / den + static_cast<double>(m2) / den;
    }

private:
    void update() noexcept
    {
        const double A = std::pow(10.0, gain / 40.0);
        g = std::tan(std::numbers::pi * freq / sr);
        k = 1.0 / Q;
        double mm0 = 0.0, mm1 = 0.0, mm2 = 0.0;
        switch (mode)
        {
        case Mode::LowPass: mm2 = 1.0; break;
        case Mode::HighPass: mm0 = 1.0; mm1 = -k; mm2 = -1.0; break;
        case Mode::BandPass: mm1 = 1.0; break;
        case Mode::Notch: mm0 = 1.0; mm1 = -k; break;
        case Mode::AllPass: mm0 = 1.0; mm1 = -2.0 * k; break;
        case Mode::Peak:
            k = 1.0 / (Q * A);
            mm0 = 1.0; mm1 = k * (A * A - 1.0);
            break;
        case Mode::LowShelf:
            g /= std::sqrt(A);
            mm0 = 1.0; mm1 = k * (A - 1.0); mm2 = A * A - 1.0;
            break;
        case Mode::HighShelf:
            g *= std::sqrt(A);
            mm0 = A * A; mm1 = k * (1.0 - A) * A; mm2 = 1.0 - A * A;
            break;
        }
        const double aa1 = 1.0 / (1.0 + g * (g + k));
        a1 = static_cast<T>(aa1);
        a2 = static_cast<T>(g * aa1);
        a3 = static_cast<T>(g * g * aa1);
        m0 = static_cast<T>(mm0);
        m1 = static_cast<T>(mm1);
        m2 = static_cast<T>(mm2);
    }

    Mode mode = Mode::LowPass;
    double sr = 48000.0, freq = 1000.0, Q = 0.7071, gain = 0.0;
    double g = 0.0, k = 1.0;
    T a1{}, a2{}, a3{}, m0{}, m1{}, m2{};
    T ic1{}, ic2{};
};
} // namespace mm
