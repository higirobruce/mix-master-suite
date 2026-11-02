#pragma once
// Building blocks shared by the reverb engines (ADR-0008): a power-of-two ring delay with integer and fractional
// reads, a Schroeder all-pass (lossless diffuser), a one-pole damping low-pass, and the orthonormal 16-point
// Hadamard butterfly. Everything is sized in prepare(); process paths never allocate.
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace mm::reverb
{
template <typename T> class RingDelay
{
public:
    void prepare(int maxSamples)
    {
        size = 1;
        while (size < maxSamples + 4)
            size <<= 1;
        mask = size - 1;
        buf.assign(static_cast<size_t>(size), T(0));
        w = 0;
    }
    void write(T x) noexcept
    {
        buf[static_cast<size_t>(w)] = x;
        w = (w + 1) & mask;
    }
    // delay d samples relative to the last written sample (d = 1 is the sample just written)
    T read(int d) const noexcept { return buf[static_cast<size_t>((w - d) & mask)]; }
    // 4-point Lagrange: linear interpolation low-passes a broadband tank signal (an extra, unlabelled decay)
    T readFrac(float d) const noexcept
    {
        const int i = std::max(1, static_cast<int>(d));
        const float f = d - static_cast<float>(i);
        const float h0 = -f * (f - 1.0f) * (f - 2.0f) / 6.0f;
        const float h1 = (f + 1.0f) * (f - 1.0f) * (f - 2.0f) / 2.0f;
        const float h2 = -(f + 1.0f) * f * (f - 2.0f) / 2.0f;
        const float h3 = (f + 1.0f) * f * (f - 1.0f) / 6.0f;
        return static_cast<T>(h0) * read(i - 1) + static_cast<T>(h1) * read(i) + static_cast<T>(h2) * read(i + 1) + static_cast<T>(h3) * read(i + 2);
    }
    void reset() noexcept
    {
        std::fill(buf.begin(), buf.end(), T(0));
        w = 0;
    }
    int capacity() const noexcept { return size - 4; }

private:
    std::vector<T> buf;
    int size = 8, mask = 7, w = 0;
};

// Schroeder all-pass: v = x + g * z, y = z - g * v, z = delayed v. Lossless for any |g| < 1.
template <typename T> class SchroederAllPass
{
public:
    void prepare(int maxSamples) { delay.prepare(maxSamples); }
    void set(int lengthSamples, T coefficient) noexcept
    {
        len = std::max(1, std::min(lengthSamples, delay.capacity()));
        g = coefficient;
    }
    T process(T x) noexcept
    {
        const T z = delay.read(len);
        const T v = x + g * z;
        delay.write(v);
        return z - g * v;
    }
    // modulated read (fractional length), for the plate's tank all-passes
    T processModulated(T x, float lengthFrac) noexcept
    {
        const T z = delay.readFrac(std::max(1.0f, lengthFrac));
        const T v = x + g * z;
        delay.write(v);
        return z - g * v;
    }
    T tap(int d) const noexcept { return delay.read(d); }
    void reset() noexcept { delay.reset(); }

private:
    RingDelay<T> delay;
    int len = 1;
    T g = T(0.5);
};

template <typename T> class OnePoleDamp
{
public:
    // cutoff -> coefficient; cutoff >= 0.49 fs means bypass
    void set(double cutoffHz, double sampleRate) noexcept
    {
        bypass = cutoffHz >= 0.49 * sampleRate;
        c = static_cast<T>(1.0 - std::exp(-2.0 * std::numbers::pi * std::min(cutoffHz, 0.49 * sampleRate) / sampleRate));
    }
    T process(T x) noexcept
    {
        if (bypass)
            return x;
        y += c * (x - y);
        return y;
    }
    void reset() noexcept { y = T(0); }

private:
    T c = T(1), y = T(0);
    bool bypass = true;
};

// Orthonormal fast Walsh-Hadamard transform of 16 values (in place): energy preserving.
inline void hadamard16(float* v) noexcept
{
    for (int h = 1; h < 16; h <<= 1)
        for (int i = 0; i < 16; i += h << 1)
            for (int j = i; j < i + h; ++j)
            {
                const float a = v[j], b = v[j + h];
                v[j] = a + b;
                v[j + h] = a - b;
            }
    for (int i = 0; i < 16; ++i)
        v[i] *= 0.25f; // 1 / sqrt(16)
}

inline double sizeScale(double size01) noexcept { return 0.25 + 0.75 * std::clamp(size01, 0.0, 1.0); }
} // namespace mm::reverb
