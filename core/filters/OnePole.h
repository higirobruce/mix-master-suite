#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace mm
{
// TPT (trapezoidal) one-pole. State in double so the integrator never stalls at the float ulp (DC residual < -200 dB).
// Bilinear: H_lp = G (1 + z^-1) / (1 - (1 - 2G) z^-1), G = g / (1 + g), g = tan(pi fc / sr); H_hp = 1 - H_lp.
template <typename T> class OnePoleLP
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setCutoff(cutoffHz);
        reset();
    }
    void setCutoff(double hz) noexcept
    {
        cutoffHz = std::clamp(hz, 0.1, 0.49 * sr);
        const double g = std::tan(std::numbers::pi * cutoffHz / sr);
        G = g / (1.0 + g);
    }
    T process(T x) noexcept
    {
        const double v = (static_cast<double>(x) - s) * G;
        const double lp = v + s;
        s = lp + v;
        return static_cast<T>(lp);
    }
    void reset() noexcept { s = 0.0; }
    std::complex<double> evaluate(double hz) const noexcept
    {
        const std::complex<double> z = std::polar(1.0, -2.0 * std::numbers::pi * hz / sr);
        return G * (1.0 + z) / (1.0 - (1.0 - 2.0 * G) * z);
    }
    double cutoff() const noexcept { return cutoffHz; }

protected:
    double sr = 48000.0;
    double cutoffHz = 1000.0;
    double G = 0.0;
    double s = 0.0;
};

template <typename T> class OnePoleHP
{
public:
    void prepare(double sampleRate) noexcept
    {
        lp.prepare(sampleRate);
        lp.setCutoff(cutoffHz);
    }
    void setCutoff(double hz) noexcept
    {
        cutoffHz = hz;
        lp.setCutoff(hz);
    }
    T process(T x) noexcept { return static_cast<T>(static_cast<double>(x) - static_cast<double>(lp.process(x))); }
    void reset() noexcept { lp.reset(); }
    std::complex<double> evaluate(double hz) const noexcept { return 1.0 - lp.evaluate(hz); }

private:
    OnePoleLP<T> lp;
    double cutoffHz = 10.0;
};
} // namespace mm
