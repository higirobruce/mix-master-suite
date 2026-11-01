#pragma once
// Static tube-style transfer: tanh with a bias offset (even-order content grows with |bias|), re-centred so
// silence stays at zero, with the small-signal gain normalised to unity. Run oversampled.
#include "core/nonlinear/Waveshapers.h"
#include <algorithm>
#include <cmath>

namespace mm
{
class TubeTransfer
{
public:
    void set(double drive, double bias) noexcept
    {
        d = static_cast<float>(std::max(drive, 1e-3));
        b = static_cast<float>(std::clamp(bias, -1.0, 1.0));
        const float sech2 = 1.0f - std::tanh(b) * std::tanh(b); // slope of tanh at the bias point
        norm = 1.0f / (d * sech2);
    }
    float process(float x) const noexcept { return shape::asym(x * d, b) * norm; }

private:
    float d = 1.0f, b = 0.0f, norm = 1.0f;
};
} // namespace mm
