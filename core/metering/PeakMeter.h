#pragma once
#include "core/common/Math.h"
#include <algorithm>
#include <cmath>

namespace mm
{
// Block peak with hold and 20 dB/s decay. Call process() per block per channel; levelDb() any time.
class PeakMeter
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }
    void process(const float* x, int n) noexcept
    {
        float blockPeak = 0.0f;
        for (int i = 0; i < n; ++i)
            blockPeak = std::max(blockPeak, std::abs(x[i]));
        const float decayDb = 20.0f * static_cast<float>(n) / static_cast<float>(sr);
        const float held = gainToDb(peak) - decayDb;
        const float heldLin = dbToGain(held);
        peak = blockPeak > heldLin ? blockPeak : heldLin;
    }
    float levelDb() const noexcept { return gainToDb(peak); }
    float levelLinear() const noexcept { return peak; }
    void reset() noexcept { peak = 0.0f; }

private:
    double sr = 48000.0;
    float peak = 0.0f;
};
} // namespace mm
