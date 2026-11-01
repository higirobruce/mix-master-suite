#pragma once
#include "core/common/Math.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace mm
{
// RMS over a sliding window (default 300 ms) using a running sum of squares. Sized in prepare().
class RmsMeter
{
public:
    void prepare(double sampleRate, double windowMs = 300.0)
    {
        window = std::max(1, static_cast<int>(std::lround(windowMs * 1e-3 * sampleRate)));
        squares.assign(static_cast<size_t>(window), 0.0);
        reset();
    }
    void process(const float* x, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double sq = static_cast<double>(x[i]) * x[i];
            sum += sq - squares[static_cast<size_t>(pos)];
            squares[static_cast<size_t>(pos)] = sq;
            if (++pos == window)
            {
                pos = 0;
                // periodic exact re-sum to stop drift
                double s = 0.0;
                for (double v : squares)
                    s += v;
                sum = s;
            }
        }
        if (sum < 0.0)
            sum = 0.0;
    }
    float levelDb() const noexcept { return gainToDb(static_cast<float>(std::sqrt(sum / window))); }
    float levelLinear() const noexcept { return static_cast<float>(std::sqrt(sum / window)); }
    void reset() noexcept
    {
        std::fill(squares.begin(), squares.end(), 0.0);
        sum = 0.0;
        pos = 0;
    }

private:
    std::vector<double> squares;
    double sum = 0.0;
    int window = 1;
    int pos = 0;
};
} // namespace mm
