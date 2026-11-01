#pragma once
// Tape hysteresis: Jiles-Atherton magnetisation model integrated per sample with RK4, after J. Chowdhury,
// "Real-time Physical Modelling for Analog Tape Machines" (DAFx 2019). Run oversampled (the nonlinear rule).
// Output is normalised by the small-signal gain so quiet signals pass at unity.
#include <algorithm>
#include <cmath>

namespace mm
{
class TapeHysteresis
{
public:
    void prepare(double sampleRate) noexcept
    {
        T = 1.0 / sampleRate;
        reset();
    }
    // drive, saturation, width in [0, 1]
    void set(double drive, double saturation, double width) noexcept
    {
        drive = std::clamp(drive, 0.0, 1.0);
        saturation = std::clamp(saturation, 0.0, 1.0);
        width = std::clamp(width, 0.0, 1.0);
        Ms = 0.5 + 1.5 * (1.0 - saturation);
        a = Ms / (0.01 + 6.0 * drive);
        c = std::sqrt(1.0 - width) - 0.01;
        k = 0.47875;
        smallSignalGain = Ms / (3.0 * a - alpha * Ms);
        nc = 1.0 - c;
        MsOverA = Ms / a;
    }

    float process(float x) noexcept
    {
        const double H = static_cast<double>(x);
        const double Hd = (H - H1) / T; // dH/dt
        // RK4 over one sample
        const double k1 = T * dMdt(M1, H1, Hd1);
        const double k2 = T * dMdt(M1 + 0.5 * k1, 0.5 * (H + H1), 0.5 * (Hd + Hd1));
        const double k3 = T * dMdt(M1 + 0.5 * k2, 0.5 * (H + H1), 0.5 * (Hd + Hd1));
        const double k4 = T * dMdt(M1 + k3, H, Hd);
        double M = M1 + (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
        if (!std::isfinite(M) || std::abs(M) > 20.0 * Ms)
            M = 0.0;
        M1 = M;
        H1 = H;
        Hd1 = Hd;
        return static_cast<float>(M / smallSignalGain);
    }

    void reset() noexcept { M1 = H1 = Hd1 = 0.0; }

private:
    static double langevin(double q) noexcept
    {
        if (std::abs(q) > 1e-4)
            return 1.0 / std::tanh(q) - 1.0 / q;
        return q / 3.0;
    }
    static double langevinD(double q) noexcept
    {
        if (std::abs(q) > 1e-4)
        {
            const double coth = 1.0 / std::tanh(q);
            return 1.0 / (q * q) - coth * coth + 1.0;
        }
        return 1.0 / 3.0;
    }
    double dMdt(double M, double H, double Hd) const noexcept
    {
        const double Q = (H + alpha * M) / a;
        const double Man = Ms * langevin(Q);
        const double Ld = langevinD(Q);
        const double deltaS = Hd > 0.0 ? 1.0 : -1.0;
        const double diff = Man - M;
        const double deltaM = ((diff > 0.0) == (Hd > 0.0)) ? 1.0 : 0.0;
        const double denom = nc * deltaS * k - alpha * diff;
        const double num = nc * deltaM * diff * Hd / (std::abs(denom) < 1e-12 ? 1e-12 : denom) + c * MsOverA * Ld * Hd;
        const double den = 1.0 - c * alpha * MsOverA * Ld;
        return num / (std::abs(den) < 1e-12 ? 1e-12 : den);
    }

    static constexpr double alpha = 1.6e-3;
    double T = 1.0 / 48000.0;
    double Ms = 1.0, a = 1.0, c = 0.7, k = 0.47875, nc = 0.3, MsOverA = 1.0, smallSignalGain = 1.0;
    double M1 = 0.0, H1 = 0.0, Hd1 = 0.0;
};
} // namespace mm
