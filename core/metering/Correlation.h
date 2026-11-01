#pragma once
#include <algorithm>
#include <cmath>

namespace mm
{
// Stereo correlation (-1..+1): Pearson coefficient of L and R over an exponential window (default 300 ms).
class CorrelationMeter
{
public:
    void prepare(double sampleRate, double windowMs = 300.0) noexcept
    {
        coeff = 1.0 - std::exp(-1.0 / (std::max(windowMs, 1.0) * 1e-3 * sampleRate));
        reset();
    }
    void process(const float* l, const float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double a = l[i], b = r[i];
            lr += (a * b - lr) * coeff;
            ll += (a * a - ll) * coeff;
            rr += (b * b - rr) * coeff;
        }
    }
    float value() const noexcept
    {
        const double den = std::sqrt(ll * rr);
        return den > 1e-12 ? static_cast<float>(std::clamp(lr / den, -1.0, 1.0)) : 0.0f;
    }
    void reset() noexcept { lr = ll = rr = 0.0; }

private:
    double coeff = 0.0, lr = 0.0, ll = 0.0, rr = 0.0;
};
} // namespace mm
