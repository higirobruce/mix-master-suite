#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace mm
{
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
inline double dbToGain(double db) noexcept { return std::pow(10.0, db * 0.05); }

inline float gainToDb(float gain, float floorDb = -120.0f) noexcept
{
    if (gain <= 0.0f)
        return floorDb;
    const float db = 20.0f * std::log10(gain);
    return db < floorDb ? floorDb : db;
}
inline double gainToDb(double gain, double floorDb = -120.0) noexcept
{
    if (gain <= 0.0)
        return floorDb;
    const double db = 20.0 * std::log10(gain);
    return db < floorDb ? floorDb : db;
}

// Fast log2 / exp2 for per-sample dB conversions on the audio thread (dynamics): log2 by exponent extraction and a
// 5-term atanh series on the mantissa (|err| < 2e-6), exp2 by a degree-6 Taylor on the fraction and an exponent
// splice (|rel err| < 2e-7). Within 1e-4 dB of the exact conversions over -130..+30 dB (tested).
inline float fastLog2(float x) noexcept
{
    if (!(x >= 1.17549435e-38f)) // zero, negative, NaN or denormal: below any audio level
        return -126.0f;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    const int e = static_cast<int>(bits >> 23) - 127;
    const float m = std::bit_cast<float>((bits & 0x007fffffu) | 0x3f800000u); // mantissa in [1, 2)
    const float t = (m - 1.0f) / (m + 1.0f);                                  // [0, 1/3)
    const float t2 = t * t;
    const float series = t * (1.0f + t2 * (1.0f / 3.0f + t2 * (1.0f / 5.0f + t2 * (1.0f / 7.0f + t2 * (1.0f / 9.0f)))));
    return static_cast<float>(e) + 2.8853900817779268f * series; // 2 / ln 2
}
inline float fastExp2(float x) noexcept
{
    x = std::clamp(x, -125.0f, 125.0f);
    const float xi = std::floor(x + 0.5f);
    const float f = (x - xi) * 0.69314718055994530942f; // ln 2 * fraction, |f| <= 0.3466
    const float p = 1.0f + f * (1.0f + f * (0.5f + f * (1.0f / 6.0f + f * (1.0f / 24.0f + f * (1.0f / 120.0f + f * (1.0f / 720.0f))))));
    const float scale = std::bit_cast<float>(static_cast<std::uint32_t>(static_cast<int>(xi) + 127) << 23); // 2^xi
    return p * scale;
}
inline float fastGainToDb(float gain, float floorDb = -120.0f) noexcept
{
    if (gain <= 0.0f)
        return floorDb;
    const float db = 6.0205999132796239f * fastLog2(gain); // 20 / log2(10)
    return db < floorDb ? floorDb : db;
}
inline float fastDbToGain(float db) noexcept { return fastExp2(db * 0.16609640474436813f); } // log2(10) / 20

template <typename T> inline bool isFiniteSample(T v) noexcept { return std::isfinite(v); }

inline double freqToOmega(double hz, double sampleRate) noexcept { return 2.0 * std::numbers::pi * hz / sampleRate; }

template <typename T> inline bool isNearlyEqual(T a, T b, T tolerance = static_cast<T>(1e-6)) noexcept
{
    return std::abs(a - b) <= tolerance * std::max(T(1), std::max(std::abs(a), std::abs(b)));
}

// Rational (Padé 7/6) tanh, within 1e-4 of std::tanh everywhere; exact saturation beyond |x| >= 4.97.
inline float fastTanh(float x) noexcept
{
    if (x >= 4.97f)
        return 1.0f;
    if (x <= -4.97f)
        return -1.0f;
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    const float y = num / den;
    return y > 1.0f ? 1.0f : (y < -1.0f ? -1.0f : y);
}

// Caches the last dB -> linear conversion so a static parameter costs one compare per block instead of a pow().
struct DbGainCache
{
    float get(float db) noexcept
    {
        if (db != lastDb)
        {
            lastDb = db;
            lastGain = dbToGain(db);
        }
        return lastGain;
    }
    float lastDb = 0.0f;
    float lastGain = 1.0f;
};
} // namespace mm
