#pragma once
// Early reflections: eight taps per pattern on the mono sum, alternating sides, times scaled by size (ADR-0008).
#include "core/reverb/ReverbParts.h"

namespace mm::reverb
{
enum class ErPattern
{
    Room,
    Hall,
    Chamber
};

template <typename T> class EarlyReflections
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        delay.prepare(static_cast<int>(std::ceil(0.12 * sr)) + 8);
        setPattern(pattern, size);
    }
    void setPattern(ErPattern p, double size01)
    {
        pattern = p;
        size = std::clamp(size01, 0.0, 1.0);
        static const double room[8] = {3.1, 5.7, 8.9, 11.6, 14.3, 17.9, 21.2, 25.4};
        static const double hall[8] = {8.2, 14.7, 22.9, 31.1, 39.4, 50.2, 61.7, 70.9};
        static const double chamber[8] = {5.3, 9.1, 14.2, 19.7, 26.4, 33.5, 39.8, 45.6};
        const double* ms = p == ErPattern::Room ? room : (p == ErPattern::Hall ? hall : chamber);
        const double scale = sizeScale(size);
        int last = 0;
        for (int k = 0; k < 8; ++k)
        {
            int t = std::max(last + 1, static_cast<int>(std::lround(ms[k] * scale * 1e-3 * sr)));
            t = std::min(t, delay.capacity() - 1);
            taps[static_cast<size_t>(k)] = t;
            last = t;
            const T g = static_cast<T>(std::pow(0.82, k + 1)); // distance falloff
            gainL[static_cast<size_t>(k)] = (k % 2 == 0) ? g : g * T(0.6);
            gainR[static_cast<size_t>(k)] = (k % 2 == 1) ? g : g * T(0.6);
        }
    }
    void processStereo(T inL, T inR, T& outL, T& outR) noexcept
    {
        delay.write(T(0.5) * (inL + inR));
        outL = outR = T(0);
        for (int k = 0; k < 8; ++k)
        {
            const T v = delay.read(taps[static_cast<size_t>(k)]);
            outL += v * gainL[static_cast<size_t>(k)];
            outR += v * gainR[static_cast<size_t>(k)];
        }
    }
    void reset() noexcept { delay.reset(); }

private:
    double sr = 48000.0, size = 0.6;
    ErPattern pattern = ErPattern::Hall;
    RingDelay<T> delay;
    std::array<int, 8> taps{};
    std::array<T, 8> gainL{}, gainR{};
};
} // namespace mm::reverb
