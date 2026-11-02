#pragma once
// First-order ADAA with a tabulated antiderivative: F on a uniform grid over [-kRange, kRange] with the shaper
// itself as the stored derivative, cubic Hermite interpolation in double (|err| ~ 3e-10 for tanh / atan at the
// 1/64 spacing). One lookup per sample (F(x1) is carried from the previous step), so a stage costs a few ns instead
// of two double transcendentals. Inputs beyond the range are clamped: every shaper here is saturated by then.
#include <array>
#include <cmath>

namespace mm
{
template <float (*Fn)(float), double (*Ad1)(double)> class TabulatedADAA1
{
public:
    static constexpr double kRange = 64.0;
    static constexpr int kPoints = 8193; // 1/64 spacing

    float process(float xIn) noexcept
    {
        const double x = std::clamp(static_cast<double>(xIn), -kRange, kRange);
        const double F = antiderivative(x);
        const double d = x - x1;
        float y;
        if (std::abs(d) < 1e-6)
            y = Fn(static_cast<float>(0.5 * (x + x1)));
        else
            y = static_cast<float>((F - F1) / d);
        x1 = x;
        F1 = F;
        return y;
    }
    void reset() noexcept
    {
        x1 = 0.0;
        F1 = antiderivative(0.0);
    }
    static double antiderivative(double x) noexcept
    {
        const Table& t = table();
        const double pos = (x + kRange) * (static_cast<double>(kPoints - 1) / (2.0 * kRange));
        int i = static_cast<int>(pos);
        if (i >= kPoints - 1)
            i = kPoints - 2;
        if (i < 0)
            i = 0;
        const double u = pos - i, h = 2.0 * kRange / (kPoints - 1);
        const double p0 = t.F[static_cast<size_t>(i)], p1 = t.F[static_cast<size_t>(i + 1)];
        const double m0 = t.f[static_cast<size_t>(i)] * h, m1 = t.f[static_cast<size_t>(i + 1)] * h;
        const double u2 = u * u, u3 = u2 * u;
        return (2.0 * u3 - 3.0 * u2 + 1.0) * p0 + (u3 - 2.0 * u2 + u) * m0 + (-2.0 * u3 + 3.0 * u2) * p1 + (u3 - u2) * m1;
    }

private:
    struct Table
    {
        std::array<double, kPoints> F{}, f{};
        Table()
        {
            for (int i = 0; i < kPoints; ++i)
            {
                const double x = -kRange + 2.0 * kRange * i / (kPoints - 1);
                F[static_cast<size_t>(i)] = Ad1(x);
                f[static_cast<size_t>(i)] = static_cast<double>(Fn(static_cast<float>(x)));
            }
        }
    };
    static const Table& table() noexcept
    {
        static const Table t; // built on first use (constructor / prepare time), never on the audio thread first
        return t;
    }
    double x1 = 0.0, F1 = 0.0;
};
} // namespace mm
