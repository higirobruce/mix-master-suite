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

// First-order shelf (bilinear, prewarped) as a generic first-order section b0 + b1 z^-1 / (1 + a1 z^-1), with the
// exact inverse available (swap numerator and denominator): the saturation's emphasis pair relies on it.
template <typename T> class FirstOrderShelf
{
public:
    // highShelf: gain above the corner; otherwise gain below it. gainDb may be negative. State in double.
    void design(double sampleRate, double cornerHz, double gainDb, bool highShelf) noexcept
    {
        const double A = std::pow(10.0, gainDb / 20.0);
        const double w = std::tan(std::numbers::pi * std::clamp(cornerHz, 1.0, 0.49 * sampleRate) / sampleRate);
        // analog: low shelf H(s) = (s + A w) / (s + w) * ... use the standard first-order forms (unity at the far side)
        double b0d, b1d, a1d;
        if (highShelf) // H(s) = (A s + w) / (s + w): gain A at high frequencies, 1 at DC
        {
            const double n = 1.0 + w;
            b0d = (A + w) / n;
            b1d = (w - A) / n;
            a1d = (w - 1.0) / n;
        }
        else // H(s) = (s + A w) / (s + w): gain A at DC, 1 at high frequencies
        {
            const double n = 1.0 + w;
            b0d = (1.0 + A * w) / n;
            b1d = (A * w - 1.0) / n;
            a1d = (w - 1.0) / n;
        }
        b0 = b0d;
        b1 = b1d;
        a1 = a1d;
    }
    void invert() noexcept // 1/H: a first-order section again (the shelf is minimum phase)
    {
        const double nb0 = 1.0 / b0, nb1 = a1 / b0, na1 = b1 / b0;
        b0 = nb0;
        b1 = nb1;
        a1 = na1;
    }
    T process(T x) noexcept
    {
        const double xd = static_cast<double>(x);
        const double y = b0 * xd + z;
        z = b1 * xd - a1 * y;
        return static_cast<T>(y);
    }
    void reset() noexcept { z = 0.0; }
    std::complex<double> evaluate(double hz, double sampleRate) const noexcept
    {
        const std::complex<double> zi = std::polar(1.0, -2.0 * std::numbers::pi * hz / sampleRate);
        return (b0 + b1 * zi) / (1.0 + a1 * zi);
    }

private:
    double b0 = 1.0, b1 = 0.0, a1 = 0.0, z = 0.0;
};
} // namespace mm
