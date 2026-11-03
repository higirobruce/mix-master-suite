#pragma once
// ITU-R BS.1770-4 / EBU R 128 loudness: K-weighting (the pre-filter and RLB high-pass re-derived from their analogue
// prototypes at any rate: the 48 k tables of the Recommendation are reproduced within 1e-5), 100 ms mean-square
// sub-blocks, momentary (400 ms) and short-term (3 s) loudness, integrated loudness with the -70 LUFS absolute and
// -10 LU relative gates over 400 ms blocks at 75 % overlap, and loudness range per EBU Tech 3342 (short-term values,
// -70 LUFS absolute / -20 LU relative gates, 10th to 95th percentile). Histories are sized in prepare() for
// maxSeconds of programme; nothing allocates afterwards. Channel weights per BS.1770 (surround channels 1.41).
#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace mm
{
class LoudnessMeter
{
public:
    // The analogue prototypes behind BS.1770-4 Tables 1 and 2 (the Recommendation gives the 48 k coefficients only),
    // in Brecht De Man's formulation (loudness.py): the RBJ shelf with the same corner, Q and gain lands 0.4 % off the
    // Table; these reproduce it to 1e-9 at 48 k and give the same filter at every other rate.
    static BiquadCoeffs<double> preFilter(double sampleRate) noexcept
    {
        const double f0 = 1681.974450955533, g = 3.999843853973347, q = 0.7071752369554196;
        const double K = std::tan(std::numbers::pi * f0 / sampleRate);
        const double Vh = std::pow(10.0, g / 20.0), Vb = std::pow(Vh, 0.499666774155);
        const double a0 = 1.0 + K / q + K * K;
        return rbj::detail::normalise(Vh + Vb * K / q + K * K, 2.0 * (K * K - Vh), Vh - Vb * K / q + K * K, a0, 2.0 * (K * K - 1.0), 1.0 - K / q + K * K);
    }
    static BiquadCoeffs<double> rlbFilter(double sampleRate) noexcept
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double K = std::tan(std::numbers::pi * f0 / sampleRate);
        const double a0 = 1.0 + K / q + K * K;
        BiquadCoeffs<double> c; // the Table keeps the numerator at 1, -2, 1 (not divided by a0)
        c.b0 = 1.0;
        c.b1 = -2.0;
        c.b2 = 1.0;
        c.a1 = 2.0 * (K * K - 1.0) / a0;
        c.a2 = (1.0 - K / q + K * K) / a0;
        return c;
    }

    static constexpr float kFloorLufs = -120.0f;

    void prepare(double sampleRate, int channels, double maxSeconds = 600.0)
    {
        sr = sampleRate;
        numChannels = std::clamp(channels, 1, kMaxChannels);
        subBlockSamples = std::max(1, static_cast<int>(std::lround(0.1 * sr)));
        const int blocks = std::max(40, static_cast<int>(maxSeconds * 10.0) + 1);
        blockPower.assign(static_cast<size_t>(blocks), 0.0);    // 400 ms block loudness (power), one per 100 ms hop
        shortTermPower.assign(static_cast<size_t>(blocks), 0.0); // 3 s loudness (power), one per 100 ms hop
        histogram.assign(static_cast<size_t>(kBins), 0);
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            pre[static_cast<size_t>(ch)].setCoeffs(preFilter(sr));
            rlb[static_cast<size_t>(ch)].setCoeffs(rlbFilter(sr));
        }
        reset();
    }

    void process(const float* const* in, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const double y = rlb[static_cast<size_t>(ch)].process(pre[static_cast<size_t>(ch)].process(static_cast<double>(in[ch][i])));
                subSum[static_cast<size_t>(ch)] += y * y;
            }
            if (++subCount == subBlockSamples)
                completeSubBlock();
        }
    }

    float momentaryLufs() const noexcept { return toLufs(windowPower(4)); }
    float shortTermLufs() const noexcept { return toLufs(windowPower(30)); }
    float maxMomentaryLufs() const noexcept { return maxMomentary; }
    float maxShortTermLufs() const noexcept { return maxShortTerm; }

    // gated integrated loudness over everything since reset (BS.1770-4 §4.2); the floor when nothing passed the gate
    float integratedLufs() const noexcept
    {
        const double absGate = fromLufs(-70.0);
        double sum = 0.0;
        int count = 0;
        for (int i = 0; i < blockCount; ++i)
        {
            const double p = blockPower[static_cast<size_t>(i)];
            if (p >= absGate)
            {
                sum += p;
                ++count;
            }
        }
        if (count == 0)
            return kFloorLufs;
        const double relGate = (sum / count) * fromLu(-10.0);
        sum = 0.0;
        count = 0;
        for (int i = 0; i < blockCount; ++i)
        {
            const double p = blockPower[static_cast<size_t>(i)];
            if (p >= absGate && p >= relGate)
            {
                sum += p;
                ++count;
            }
        }
        return count == 0 ? kFloorLufs : toLufs(sum / count);
    }

    // EBU Tech 3342: short-term values gated at -70 LUFS absolute and -20 LU relative; LRA = 95th - 10th percentile
    float loudnessRangeLu() const noexcept
    {
        const double absGate = fromLufs(-70.0);
        double sum = 0.0;
        int count = 0;
        for (int i = 0; i < shortTermCount; ++i)
            if (shortTermPower[static_cast<size_t>(i)] >= absGate)
            {
                sum += shortTermPower[static_cast<size_t>(i)];
                ++count;
            }
        if (count == 0)
            return 0.0f;
        const double relGate = (sum / count) * fromLu(-20.0);
        // percentiles from a 0.01 LU histogram (no sort on the audio thread's data)
        std::fill(histogram.begin(), histogram.end(), 0);
        int total = 0;
        for (int i = 0; i < shortTermCount; ++i)
        {
            const double p = shortTermPower[static_cast<size_t>(i)];
            if (p >= absGate && p >= relGate)
            {
                ++histogram[static_cast<size_t>(binOf(toLufs(p)))];
                ++total;
            }
        }
        if (total == 0)
            return 0.0f;
        const double p10 = percentile(0.10, total), p95 = percentile(0.95, total);
        return static_cast<float>(p95 - p10);
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            pre[static_cast<size_t>(ch)].reset();
            rlb[static_cast<size_t>(ch)].reset();
            subSum[static_cast<size_t>(ch)] = 0.0;
        }
        std::fill(recent.begin(), recent.end(), 0.0);
        recentHead = 0;
        recentCount = 0;
        subCount = 0;
        blockCount = 0;
        shortTermCount = 0;
        hop = 0;
        maxMomentary = maxShortTerm = kFloorLufs;
    }

private:
    static constexpr int kMaxChannels = 5;
    static constexpr int kRecent = 30; // 3 s of 100 ms sub-blocks
    static constexpr int kBins = 12000; // -120 .. 0 LUFS in 0.01 LU
    static constexpr double kWeights[kMaxChannels] = {1.0, 1.0, 1.0, 1.41, 1.41};

    static double fromLufs(double lufs) noexcept { return std::pow(10.0, (lufs + 0.691) / 10.0); }
    static double fromLu(double lu) noexcept { return std::pow(10.0, lu / 10.0); }
    static float toLufs(double power) noexcept
    {
        return power > 0.0 ? std::max(kFloorLufs, static_cast<float>(-0.691 + 10.0 * std::log10(power))) : kFloorLufs;
    }
    static int binOf(float lufs) noexcept { return std::clamp(static_cast<int>(std::lround((lufs + 120.0f) * 100.0f)), 0, kBins - 1); }

    double percentile(double q, int total) const noexcept
    {
        const int target = std::max(1, static_cast<int>(std::ceil(q * total)));
        int acc = 0;
        for (int b = 0; b < kBins; ++b)
        {
            acc += histogram[static_cast<size_t>(b)];
            if (acc >= target)
                return b / 100.0 - 120.0;
        }
        return 0.0;
    }

    // mean weighted power over the last `blocks` sub-blocks (0 if fewer have completed)
    double windowPower(int blocks) const noexcept
    {
        if (recentCount < blocks)
            return 0.0;
        double sum = 0.0;
        for (int j = 0; j < blocks; ++j)
        {
            int idx = recentHead - 1 - j;
            if (idx < 0)
                idx += kRecent;
            sum += recent[static_cast<size_t>(idx)];
        }
        return sum / blocks;
    }

    void completeSubBlock() noexcept
    {
        double z = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            z += kWeights[ch] * subSum[static_cast<size_t>(ch)] / subBlockSamples;
            subSum[static_cast<size_t>(ch)] = 0.0;
        }
        subCount = 0;
        recent[static_cast<size_t>(recentHead)] = z;
        if (++recentHead == kRecent)
            recentHead = 0;
        recentCount = std::min(recentCount + 1, kRecent);
        ++hop;
        if (recentCount >= 4) // a 400 ms block completes every 100 ms once four sub-blocks exist
        {
            const double m = windowPower(4);
            maxMomentary = std::max(maxMomentary, toLufs(m));
            if (blockCount < static_cast<int>(blockPower.size()))
                blockPower[static_cast<size_t>(blockCount++)] = m;
        }
        if (recentCount >= kRecent)
        {
            const double s = windowPower(kRecent);
            maxShortTerm = std::max(maxShortTerm, toLufs(s));
            if (shortTermCount < static_cast<int>(shortTermPower.size()))
                shortTermPower[static_cast<size_t>(shortTermCount++)] = s;
        }
    }

    double sr = 48000.0;
    int numChannels = 2, subBlockSamples = 4800, subCount = 0;
    std::array<Biquad<double>, kMaxChannels> pre, rlb;
    std::array<double, kMaxChannels> subSum{};
    std::array<double, kRecent> recent{};
    int recentHead = 0, recentCount = 0, hop = 0;
    std::vector<double> blockPower, shortTermPower;
    int blockCount = 0, shortTermCount = 0;
    mutable std::vector<int> histogram;
    float maxMomentary = kFloorLufs, maxShortTerm = kFloorLufs;
};
} // namespace mm
