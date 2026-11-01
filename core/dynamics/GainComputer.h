#pragma once
#include <algorithm>
#include <cmath>

namespace mm
{
struct GainComputerParams
{
    double thresholdDb = 0.0;
    double ratio = 1.0;   // >= 1; >= 1000 is treated as infinity
    double kneeDb = 0.0;  // total knee width
    bool expander = false;
    double rangeDb = -80.0; // expander floor (negative)
};

// Static gain curve in dB (log-domain). Compressor: soft knee over [T - k/2, T + k/2] (quadratic), slope 1/R above.
// Expander: gain = (R - 1)(L - T) below threshold, floored at rangeDb.
template <typename T> class GainComputer
{
public:
    void set(const GainComputerParams& p) noexcept
    {
        params = p;
        params.ratio = std::max(p.ratio, 1.0);
        params.kneeDb = std::max(p.kneeDb, 0.0);
        slope = params.ratio >= 1000.0 ? -1.0 : (1.0 / params.ratio - 1.0); // gain change per dB over threshold
    }

    T gainDb(T levelDb) const noexcept
    {
        const double L = static_cast<double>(levelDb), Tt = params.thresholdDb, k = params.kneeDb;
        if (params.expander)
        {
            const double g = L >= Tt ? 0.0 : (params.ratio - 1.0) * (L - Tt);
            return static_cast<T>(std::max(g, params.rangeDb));
        }
        const double over = L - Tt;
        double g;
        if (k <= 0.0 || over <= -0.5 * k)
            g = over <= 0.0 ? 0.0 : slope * over;
        else if (over >= 0.5 * k)
            g = slope * over;
        else
        {
            const double x = over + 0.5 * k;
            g = slope * x * x / (2.0 * k);
        }
        return static_cast<T>(g);
    }

private:
    GainComputerParams params;
    double slope = 0.0;
};
} // namespace mm
