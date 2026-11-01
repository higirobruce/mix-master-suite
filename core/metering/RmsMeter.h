#pragma once
#include "core/common/Math.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace mm
{
// RMS over a sliding window (default 300 ms) kept as 64 fixed bins of sums of squares plus the bin being filled,
// so memory is constant (no per-sample ring) and the per-sample cost is one multiply-add.
class RmsMeter
{
public:
    void prepare(double sampleRate, double windowMs = 300.0) noexcept
    {
        window = std::max(kBins, static_cast<int>(std::lround(windowMs * 1e-3 * sampleRate)));
        binLen = (window + kBins - 1) / kBins;
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
        if (n <= 0 || !(sumSquares >= 0.0))
            return;
        // spread the block evenly over the bins it covers (block sizes rarely align with bin boundaries)
        while (n > 0)
        {
            const int take = std::min(n, binLen - curCount);
            curSum += sumSquares * (static_cast<double>(take) / static_cast<double>(n));
            sumSquares -= sumSquares * (static_cast<double>(take) / static_cast<double>(n));
            curCount += take;
            n -= take;
            if (curCount >= binLen)
            {
                total -= bins[static_cast<size_t>(head)].sum;
                totalCount -= bins[static_cast<size_t>(head)].count;
                bins[static_cast<size_t>(head)] = {curSum, curCount};
                total += curSum;
                totalCount += curCount;
                head = (head + 1) % kBins;
                curSum = 0.0;
                curCount = 0;
            }
        }
        if (total < 0.0)
            total = 0.0;
    }
    // RMS of the samples seen so far (up to the window), so two meters at different fill levels stay comparable.
    float levelLinear() const noexcept
    {
        const double sum = total + curSum;
        const int count = totalCount + curCount;
        return count > 0 ? static_cast<float>(std::sqrt(sum / static_cast<double>(count))) : 0.0f;
    }
    float levelDb() const noexcept { return gainToDb(levelLinear()); }
    void reset() noexcept
    {
        bins.fill({});
        total = curSum = 0.0;
        totalCount = curCount = 0;
        head = 0;
    }

private:
    static constexpr int kBins = 64;
    struct Bin
    {
        double sum = 0.0;
        int count = 0;
    };
    std::array<Bin, static_cast<size_t>(kBins)> bins{};
    double total = 0.0, curSum = 0.0;
    int totalCount = 0, curCount = 0;
    int window = 64, binLen = 1, head = 0;
};
} // namespace mm
