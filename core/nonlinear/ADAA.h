#pragma once
// First-order antiderivative anti-aliasing (Parker et al. 2016): y = (F(x) - F(x1)) / (x - x1), with the plain
// function at the midpoint when consecutive samples are (nearly) equal. Adds half a sample of delay.
#include <cmath>

namespace mm
{
template <typename Fn, typename Ad> inline float adaa1Step(Fn&& fn, Ad&& ad, float x, float& x1) noexcept
{
    const float d = x - x1;
    const float y = std::abs(d) < 1e-5f ? fn(0.5f * (x + x1)) : (ad(x) - ad(x1)) / d;
    x1 = x;
    return y;
}

template <float (*Fn)(float), float (*Ad1)(float)> class ADAA1
{
public:
    float process(float x) noexcept { return adaa1Step(Fn, Ad1, x, x1); }
    void reset() noexcept { x1 = 0.0f; }

private:
    float x1 = 0.0f;
};
} // namespace mm
