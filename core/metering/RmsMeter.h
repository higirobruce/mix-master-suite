#pragma once
#include "core/common/Math.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace mm
{
// RMS over a sliding window (default 300 ms). Per-call sums of squares are kept in a ring (one entry per call), so
// the per-sample cost is one multiply-add and the window granularity is the block size.
class RmsMeter
{
public:
    void prepare(double sampleRate, double windowMs = 300.0)
    {
        window = std::max(1, static_cast<int>(std::lround(windowMs * 1e-3 * sampleRate)));
        entries.assign(static_cast<size_t>(window) + 1, {});
        reset();
    }
    void process(const float* x, int n) noexcept
    {
        double sq = 0.0;
        for (int i = 0; i < n; ++i)
            sq += static_cast<double>(x[i]) * x[i];
        processSumSquares(sq, n);
    }
    void processSumSquares(double sumSquares, int n) noexcept
    {
        if (n <= 0)
            return;
        entries[static_cast<size_t>(head)] = {sumSquares, n};
        head = (head + 1) % static_cast<int>(entries.size());
        sum += sumSquares;
        count += n;
        while (count > window && tail != head)
        {
            const auto& e = entries[static_cast<size_t>(tail)];
            if (count - e.n < window)
                break;
            sum -= e.sum;
            count -= e.n;
            tail = (tail + 1) % static_cast<int>(entries.size());
        }
        if (head == tail) // ring full (tiny blocks): drop the oldest
        {
            const auto& e = entries[static_cast<size_t>(tail)];
            sum -= e.sum;
            count -= e.n;
            tail = (tail + 1) % static_cast<int>(entries.size());
        }
        if (sum < 0.0)
            sum = 0.0;
    }
    float levelLinear() const noexcept
    {
        return count > 0 ? static_cast<float>(std::sqrt(sum / static_cast<double>(std::max(count, window)))) : 0.0f;
    }
    float levelDb() const noexcept { return gainToDb(levelLinear()); }
    void reset() noexcept
    {
        std::fill(entries.begin(), entries.end(), Entry{});
        sum = 0.0;
        count = 0;
        head = tail = 0;
    }

private:
    struct Entry
    {
        double sum = 0.0;
        int n = 0;
    };
    std::vector<Entry> entries;
    double sum = 0.0;
    int count = 0;
    int window = 1;
    int head = 0, tail = 0;
};
} // namespace mm
