#pragma once
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
