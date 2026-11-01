#pragma once
#include "core/common/Math.h"
#include <algorithm>
#include <cmath>

namespace mm
{
// Block peak with hold and 20 dB/s decay. Feed either samples (process) or a precomputed block peak (processPeak).
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
        processPeak(blockPeak, n);
    }
    void processPeak(float blockPeak, int n) noexcept
    {
        const float decay = decayPerSample(n);
        const float held = peak * decay;
        peak = blockPeak > held ? blockPeak : held;
    }
    float levelDb() const noexcept { return gainToDb(peak); }
    float levelLinear() const noexcept { return peak; }
    void reset() noexcept { peak = 0.0f; }

private:
    float decayPerSample(int n) noexcept
    {
        // 20 dB/s -> linear factor per n samples; cached for the common case of a constant block size
        if (n != cachedN)
        {
            cachedN = n;
            cachedDecay = std::pow(10.0f, -20.0f * static_cast<float>(n) / static_cast<float>(sr) / 20.0f);
        }
        return cachedDecay;
    }
    double sr = 48000.0;
    float peak = 0.0f;
    int cachedN = -1;
    float cachedDecay = 1.0f;
};
} // namespace mm
