#pragma once
// Polyphase half-band FIR oversampler (2x / 4x / 8x), linear phase, integer latency. Each stage is a Kaiser
// half-band (core/filters/FIRDesign.h) folded into its symmetric odd taps: an up-sampler costs P multiplies per
// input sample, a down-sampler P per output sample (P = 24 for the first stage, 6 for the others). Stage 0 keeps a
// passband to 0.44 fs (transition 0.06, ~88 dB stopband: a 19 kHz tone at 44.1 k must pass without an image);
// later stages only see that band and use a 0.25 transition.
// Replaces juce::dsp::Oversampling (its maximum-quality design spends ~5x the taps for a generic 5 % transition).
// Factor 1 is a passthrough with zero latency. Sized in prepare(); no allocation afterwards.
#include <cstring>
#include "core/filters/FIRDesign.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace mm
{
class Oversampler
{
public:
    void prepare(double sampleRate, int maxBlock, int channels, int factor)
    {
        sr = sampleRate;
        fac = factor >= 8 ? 8 : (factor >= 4 ? 4 : (factor >= 2 ? 2 : 1));
        numChannels = std::max(1, channels);
        numStages = fac == 8 ? 3 : (fac == 4 ? 2 : (fac == 2 ? 1 : 0));
        stages.clear();
        buffers.clear();
        int rateMul = 1;
        long totalDelayOs = 0; // in samples at the oversampled rate
        for (int s = 0; s < numStages; ++s)
        {
            Stage st;
            const int taps = s == 0 ? 95 : 23;
            st.design(taps, s == 0 ? 0.06 : 0.25, numChannels, std::max(1, maxBlock) * rateMul);
            stages.push_back(std::move(st));
            rateMul *= 2;
            totalDelayOs += 2L * stages.back().M * (fac / rateMul); // up + down, scaled to the oversampled rate
        }
        for (int s = 0; s <= numStages; ++s)
            buffers.emplace_back(numChannels, std::max(1, maxBlock) * (1 << s));
        pad = numStages == 0 ? 0 : static_cast<int>((fac - totalDelayOs % fac) % fac); // < fac <= 8
        latency = numStages == 0 ? 0 : static_cast<int>((totalDelayOs + pad) / fac);
        padCarry.assign(static_cast<size_t>(numChannels * kMaxPad), 0.0f);
        reset();
    }

    // Up-sample `in` (n samples) into the internal buffer; returns a block of n * factor samples.
    juce::dsp::AudioBlock<float> up(juce::dsp::AudioBlock<float> in) noexcept
    {
        if (numStages == 0)
            return in;
        const int n = static_cast<int>(in.getNumSamples());
        const int ch = std::min(static_cast<int>(in.getNumChannels()), numChannels);
        for (int s = 0; s < numStages; ++s)
        {
            const int len = n << s;
            for (int c = 0; c < ch; ++c)
            {
                const float* src = s == 0 ? in.getChannelPointer(static_cast<size_t>(c)) : buffers[static_cast<size_t>(s)].getReadPointer(c);
                stages[static_cast<size_t>(s)].upsample(src, buffers[static_cast<size_t>(s + 1)].getWritePointer(c), len, c);
            }
        }
        return juce::dsp::AudioBlock<float>(buffers[static_cast<size_t>(numStages)]).getSubsetChannelBlock(0, static_cast<size_t>(ch)).getSubBlock(0, static_cast<size_t>(n * fac));
    }

    // Down-sample the internal buffer back into `out` (n samples, the block passed to up()).
    void down(juce::dsp::AudioBlock<float> out) noexcept
    {
        if (numStages == 0)
            return;
        const int n = static_cast<int>(out.getNumSamples());
        const int ch = std::min(static_cast<int>(out.getNumChannels()), numChannels);
        if (pad > 0) // integer-latency padding: shift the block by `pad` samples, carrying the tail to the next block
            for (int c = 0; c < ch; ++c)
            {
                float* p = buffers[static_cast<size_t>(numStages)].getWritePointer(c);
                float* carry = padCarry.data() + static_cast<size_t>(c * kMaxPad);
                const int len = n * fac;
                float tail[kMaxPad];
                std::copy(p + len - pad, p + len, tail);
                std::memmove(p + pad, p, static_cast<size_t>(len - pad) * sizeof(float));
                std::copy(carry, carry + pad, p);
                std::copy(tail, tail + pad, carry);
            }
        for (int s = numStages - 1; s >= 0; --s)
        {
            const int len = n << s; // output length of this stage
            for (int c = 0; c < ch; ++c)
            {
                float* dst = s == 0 ? out.getChannelPointer(static_cast<size_t>(c)) : buffers[static_cast<size_t>(s)].getWritePointer(c);
                stages[static_cast<size_t>(s)].downsample(buffers[static_cast<size_t>(s + 1)].getReadPointer(c), dst, len, c);
            }
        }
    }

    int latencySamples() const noexcept { return latency; }
    double oversampledRate() const noexcept { return sr * fac; }
    int factor() const noexcept { return fac; }
    void reset() noexcept
    {
        for (auto& s : stages)
            s.reset();
        for (auto& b : buffers)
            b.clear();
        std::fill(padCarry.begin(), padCarry.end(), 0.0f);
    }

private:
    // One half-band stage. Both directions reduce to the same kernel on a contiguous padded array A with the block
    // appended to the last M samples of history: acc[i] = sum_t c_t (A[i-k-1-t] + A[i-k+t]), a tap-outer / sample-inner
    // loop the compiler vectorises (sliding-window dot products per sample would not).
    struct Stage
    {
        std::vector<float> c; // folded odd-offset taps c[i] = h[M ± (2i+1)], i = 0..P-1
        int P = 0, M = 0, k = 0, maxIn = 0;
        std::vector<float> histUp, histE, histO; // per channel: M samples each
        std::vector<float> padA, padO, acc;      // shared scratch: M + maxIn

        void design(int taps, double transition, int channels, int maxInputSamples)
        {
            const auto h = fir::halfBand(taps, transition);
            const int N = static_cast<int>(h.size());
            M = (N - 1) / 2; // odd
            k = (M - 1) / 2;
            P = (N + 1) / 4;
            c.resize(static_cast<size_t>(P));
            for (int i = 0; i < P; ++i)
                c[static_cast<size_t>(i)] = static_cast<float>(h[static_cast<size_t>(M + 2 * i + 1)]);
            maxIn = std::max(1, maxInputSamples);
            histUp.assign(static_cast<size_t>(channels * M), 0.0f);
            histE.assign(static_cast<size_t>(channels * M), 0.0f);
            histO.assign(static_cast<size_t>(channels * M), 0.0f);
            padA.assign(static_cast<size_t>(M + maxIn), 0.0f);
            padO.assign(static_cast<size_t>(M + maxIn), 0.0f);
            acc.assign(static_cast<size_t>(maxIn), 0.0f);
        }
        void reset() noexcept
        {
            std::fill(histUp.begin(), histUp.end(), 0.0f);
            std::fill(histE.begin(), histE.end(), 0.0f);
            std::fill(histO.begin(), histO.end(), 0.0f);
        }
        // acc[i] = sum_t c_t (A[i-k-1-t] + A[i-k+t]) for i in [0, n), A = padA + M
        // acc[i] = sum_t c_t (A[i-k-1-t] + A[i-k+t]) for i in [0, n): one SIMD pass per tap (JUCE vector ops, SSE/NEON on
        // every compiler); the compiler-vectorised alternatives chose the tap axis and ran scalar.
        void kernel(int n) noexcept
        {
            const float* A = padA.data() + M;
            float* out = acc.data();
            juce::FloatVectorOperations::clear(out, n);
            for (int t = 0; t < P; ++t)
            {
                const float ct = c[static_cast<size_t>(t)];
                juce::FloatVectorOperations::addWithMultiply(out, A - k - 1 - t, ct, n);
                juce::FloatVectorOperations::addWithMultiply(out, A - k + t, ct, n);
            }
        }
        // x (n samples) -> y (2n samples): y[2n] = 2 sum_i c_i (x[n-k-i-1] + x[n-k+i]), y[2n+1] = x[n-k]
        void upsample(const float* x, float* y, int n, int ch) noexcept
        {
            float* hist = histUp.data() + static_cast<size_t>(ch * M);
            std::copy(hist, hist + M, padA.begin());
            std::copy(x, x + n, padA.begin() + M);
            kernel(n);
            const float* A = padA.data() + M;
            for (int i = 0; i < n; ++i)
            {
                y[2 * i] = 2.0f * acc[static_cast<size_t>(i)];
                y[2 * i + 1] = A[i - k];
            }
            std::copy(padA.begin() + n, padA.begin() + n + M, hist); // last M inputs
        }
        // y (2n samples) -> x (n samples): with Ye = y[2i], Yo = y[2i+1]:
        // x[i] = 0.5 Yo[i-k-1] + sum_t c_t (Ye[i-k-1-t] + Ye[i-k+t])
        void downsample(const float* y, float* x, int n, int ch) noexcept
        {
            float* he = histE.data() + static_cast<size_t>(ch * M);
            float* ho = histO.data() + static_cast<size_t>(ch * M);
            std::copy(he, he + M, padA.begin());
            std::copy(ho, ho + M, padO.begin());
            for (int i = 0; i < n; ++i)
            {
                padA[static_cast<size_t>(M + i)] = y[2 * i];
                padO[static_cast<size_t>(M + i)] = y[2 * i + 1];
            }
            kernel(n);
            const float* O = padO.data() + M;
            for (int i = 0; i < n; ++i)
                x[i] = acc[static_cast<size_t>(i)] + 0.5f * O[i - k - 1];
            std::copy(padA.begin() + n, padA.begin() + n + M, he);
            std::copy(padO.begin() + n, padO.begin() + n + M, ho);
        }
    };

    std::vector<Stage> stages;
    std::vector<juce::AudioBuffer<float>> buffers; // buffers[s] holds the signal at 2^s x the host rate
    static constexpr int kMaxPad = 8;
    std::vector<float> padCarry;
    double sr = 48000.0;
    int fac = 1, numStages = 0, numChannels = 1, latency = 0, pad = 0;
};
} // namespace mm
