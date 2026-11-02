#pragma once
// 16-line feedback delay network (ADR-0008): Hadamard feedback matrix, per-line gains g_i = 10^(-3 L_i / (RT60 fs)),
// per-line one-pole damping, four Schroeder input diffusers, mutually prime line lengths scaled by size.
// Odd lines feed the left output, even lines the right; L and R inputs enter alternating lines.
#include "core/reverb/ReverbParts.h"
#include <numeric>

namespace mm::reverb
{
enum class FdnAlgorithm
{
    Room,
    Hall,
    Chamber
};

template <typename T> class FDN
{
public:
    static constexpr int N = 16;

    void prepare(double sampleRate, double maxLineSeconds)
    {
        sr = sampleRate;
        const int cap = static_cast<int>(std::ceil(maxLineSeconds * sr)) + 8;
        for (auto& d : lines)
            d.prepare(cap);
        static const double apMs[4] = {4.771, 3.595, 12.73, 9.307};
        for (int i = 0; i < 4; ++i)
        {
            diffusers[static_cast<size_t>(i)].prepare(static_cast<int>(std::ceil(0.02 * sr)));
            diffusers[static_cast<size_t>(i)].set(static_cast<int>(std::lround(apMs[i] * 1e-3 * sr)), diffusionCoeff);
        }
        setAlgorithm(algorithm, size);
        setDamping(damping);
        reset();
    }

    // Lengths glide to their new targets over kGlideSeconds (fractional reads while moving), so a size change never
    // restarts the tail; the targets are mutually prime sample counts.
    static constexpr double kGlideSeconds = 0.2;
    void setSize(double size01) { setAlgorithm(algorithm, size01); }
    void setAlgorithm(FdnAlgorithm a, double size01)
    {
        algorithm = a;
        size = std::clamp(size01, 0.0, 1.0);
        double lo = 0.029, hi = 0.137; // Hall
        if (a == FdnAlgorithm::Room) { lo = 0.011; hi = 0.043; }
        if (a == FdnAlgorithm::Chamber) { lo = 0.017; hi = 0.083; }
        const double scale = sizeScale(size);
        // geometric spread, then nudge to mutually prime sample counts (ascending)
        for (int i = 0; i < N; ++i)
        {
            const double sec = lo * std::pow(hi / lo, static_cast<double>(i) / (N - 1)) * scale;
            int len = std::max(48, static_cast<int>(std::lround(sec * sr)));
            len = std::min(len, lines[0].capacity());
            lengths[static_cast<size_t>(i)] = len;
        }
        for (int i = 1; i < N; ++i)
        {
            int& len = lengths[static_cast<size_t>(i)];
            if (len <= lengths[static_cast<size_t>(i - 1)])
                len = lengths[static_cast<size_t>(i - 1)] + 1;
            auto coprime = [&](int candidate) {
                for (int j = 0; j < i; ++j)
                    if (std::gcd(candidate, lengths[static_cast<size_t>(j)]) != 1)
                        return false;
                return true;
            };
            while (!coprime(len))
                ++len;
        }
        for (int i = 0; i < N; ++i)
        {
            const double target = static_cast<double>(lengths[static_cast<size_t>(i)]);
            if (!started)
                curLen[static_cast<size_t>(i)] = target;
            glideInc[static_cast<size_t>(i)] = (target - curLen[static_cast<size_t>(i)]) / std::max(1.0, kGlideSeconds * sr);
        }
        started = true;
        updateGains();
    }
    void setRT60(double seconds)
    {
        rt60 = std::max(0.05, seconds);
        updateGains();
        setDiffusion(diffusion01); // diffuser ring time scales down with short decays
    }
    void setDamping(float d01)
    {
        damping = std::clamp(d01, 0.0f, 1.0f);
        const double fc = damping <= 0.0f ? sr : 20000.0 * std::pow(1000.0 / 20000.0, static_cast<double>(damping));
        for (auto& dmp : damp)
            dmp.set(fc, sr);
    }
    void setDiffusion(float d01)
    {
        diffusion01 = std::clamp(d01, 0.0f, 1.0f);
        // a 12.7 ms all-pass at g 0.675 rings for ~0.2 s on its own: below RT60 0.6 s shorten and soften the diffusers
        const double scale = std::clamp(rt60 / 0.6, 0.25, 1.0);
        diffusionCoeff = static_cast<T>((0.5 + 0.25 * diffusion01) * scale);
        static const double apMs[4] = {4.771, 3.595, 12.73, 9.307};
        for (int i = 0; i < 4; ++i)
            diffusers[static_cast<size_t>(i)].set(std::max(1, static_cast<int>(std::lround(apMs[i] * scale * 1e-3 * sr))), diffusionCoeff);
    }
    const std::array<int, N>& lengthsSamples() const noexcept { return lengths; }

    void processStereo(T inL, T inR, T& outL, T& outR) noexcept
    {
        const T aL = diffusers[1].process(diffusers[0].process(inL * T(0.5)));
        const T aR = diffusers[3].process(diffusers[2].process(inR * T(0.5)));
        float v[N];
        T sumL = T(0), sumR = T(0);
        for (int i = 0; i < N; ++i)
        {
            double& cur = curLen[static_cast<size_t>(i)];
            const double target = static_cast<double>(lengths[static_cast<size_t>(i)]);
            T y;
            if (cur == target)
                y = lines[static_cast<size_t>(i)].read(lengths[static_cast<size_t>(i)]);
            else
            {
                const double inc = glideInc[static_cast<size_t>(i)];
                cur += inc;
                if ((inc > 0.0 && cur >= target) || (inc < 0.0 && cur <= target))
                    cur = target;
                y = lines[static_cast<size_t>(i)].readFrac(static_cast<float>(cur));
            }
            if (i & 1) sumL += y; else sumR += y;
            v[i] = static_cast<float>(damp[static_cast<size_t>(i)].process(y) * gains[static_cast<size_t>(i)]);
        }
        hadamard16(v);
        for (int i = 0; i < N; ++i)
            lines[static_cast<size_t>(i)].write(static_cast<T>(v[i]) + ((i & 1) ? aL : aR));
        outL = sumL * T(0.35355339); // 1 / sqrt(8)
        outR = sumR * T(0.35355339);
    }

    void reset() noexcept
    {
        for (int i = 0; i < N; ++i)
            curLen[static_cast<size_t>(i)] = static_cast<double>(lengths[static_cast<size_t>(i)]);
        for (auto& d : lines) d.reset();
        for (auto& d : damp) d.reset();
        for (auto& a : diffusers) a.reset();
    }

private:
    void updateGains() noexcept
    {
        for (int i = 0; i < N; ++i)
            gains[static_cast<size_t>(i)] = static_cast<T>(std::pow(10.0, -3.0 * lengths[static_cast<size_t>(i)] / (rt60 * sr)));
    }
    double sr = 48000.0, rt60 = 2.0, size = 0.6;
    float damping = 0.0f, diffusion01 = 0.7f;
    bool started = false;
    std::array<double, N> curLen{}, glideInc{};
    T diffusionCoeff = T(0.675);
    FdnAlgorithm algorithm = FdnAlgorithm::Hall;
    std::array<RingDelay<T>, N> lines;
    std::array<OnePoleDamp<T>, N> damp;
    std::array<SchroederAllPass<T>, 4> diffusers;
    std::array<int, N> lengths{};
    std::array<T, N> gains{};
};
} // namespace mm::reverb
