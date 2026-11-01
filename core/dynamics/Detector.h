#pragma once
#include <algorithm>
#include <cmath>

namespace mm
{
enum class DetectorMode
{
    Peak,
    Rms
};

// Level detector: Peak = |x|; Rms = sqrt of a one-pole mean square with the window as time constant.
// Returns a linear level >= 0 per sample. The true-peak detector (oversampled) lives with the limiter (M5).
template <typename T> class Detector
{
public:
    using Mode = DetectorMode;
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setRmsWindowMs(windowMs);
        reset();
    }
    void setMode(DetectorMode m) noexcept { mode = m; }
    void setRmsWindowMs(double ms) noexcept
    {
        windowMs = std::max(ms, 0.01);
        coeff = 1.0 - std::exp(-1.0 / (windowMs * 1e-3 * sr));
    }
    T process(T x) noexcept
    {
        if (mode == DetectorMode::Peak)
            return std::abs(x);
        meanSquare += (static_cast<double>(x) * x - meanSquare) * coeff;
        return static_cast<T>(std::sqrt(meanSquare));
    }
    void reset() noexcept { meanSquare = 0.0; }

private:
    DetectorMode mode = DetectorMode::Peak;
    double sr = 48000.0, windowMs = 50.0, coeff = 0.0, meanSquare = 0.0;
};
} // namespace mm
