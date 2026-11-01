#pragma once
#include <algorithm>
#include <cmath>
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
