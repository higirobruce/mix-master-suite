#pragma once
// TPDF dither and quantisation to 16 or 24 bits: the last operation of a delivery chain. Two uniform draws from a
// xorshift generator give the triangular noise (+-1 LSB peak), then the sample is rounded to the grid. No state
// but the generator; nothing allocates.
#include <cmath>
#include <cstdint>

namespace mm
{
class Dither
{
public:
    void setBits(int bits) noexcept
    {
        quantum = bits > 0 ? static_cast<float>(1u << (bits - 1)) : 0.0f;
        enabled = bits > 0;
    }
    bool isEnabled() const noexcept { return enabled; }
    void seed(std::uint32_t s) noexcept { state = s == 0 ? 0x9e3779b9u : s; }

    float process(float x) noexcept
    {
        if (!enabled)
            return x;
        const float lsb = 1.0f / quantum;
        const float r = (uniform() + uniform() - 1.0f) * lsb; // triangular, +-1 LSB
        return std::round((x + r) * quantum) / quantum;
    }

private:
    float uniform() noexcept // [0, 1)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
    }
    float quantum = 32768.0f;
    bool enabled = false;
    std::uint32_t state = 0x9e3779b9u;
};
} // namespace mm
