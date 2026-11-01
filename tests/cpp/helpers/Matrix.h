#pragma once
#include <algorithm>

namespace mm::test
{
inline constexpr double kMatrixRates[] = {44100.0, 48000.0, 96000.0, 192000.0};

struct Band
{
    double lowHz, highHz;
};
// Measurement band for the ±0.1 dB / ±2° checks: 20 Hz to min(20 kHz, 0.45·fs)
inline Band measureBand(double sr) { return {20.0, std::min(20000.0, 0.45 * sr)}; }
} // namespace mm::test
