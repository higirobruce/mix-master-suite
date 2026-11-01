#pragma once
// First-order antiderivative anti-aliasing (Parker et al. 2016): y = (F(x) - F(x1)) / (x - x1), with the plain
// function at the midpoint when consecutive samples are (nearly) equal. The antiderivative difference is computed
// in double: in float the cancellation leaves a signal-correlated error of -60 dBFS on bass. Adds half a sample of delay.
#include <cmath>

namespace mm
{
// fn: float(float) shaper; ad: double(double) antiderivative; x1 carried as double.
template <typename Fn, typename Ad> inline float adaa1Step(Fn&& fn, Ad&& ad, float x, double& x1) noexcept
{
    const double xd = static_cast<double>(x);
    const double d = xd - x1;
    const float y = std::abs(d) < 1e-6 ? fn(static_cast<float>(0.5 * (xd + x1))) : static_cast<float>((ad(xd) - ad(x1)) / d);
    x1 = xd;
    return y;
}

template <float (*Fn)(float), double (*Ad1)(double)> class ADAA1
{
public:
    float process(float x) noexcept { return adaa1Step(Fn, Ad1, x, x1); }
    void reset() noexcept { x1 = 0.0; }

private:
    double x1 = 0.0;
};
} // namespace mm
