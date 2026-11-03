#pragma once
// True-peak meter (ITU-R BS.1770-4 Annex 2): the signal is oversampled 4x (2x from 96 k upward, where the Annex finds
// it sufficient) through the core polyphase half-band oversampler (linear phase, ~0.01 dB passband ripple, far
// tighter than the Annex's 48-tap example filter) and the peak at the oversampled rate is read in dBTP. Sized in
// prepare(); processBlock() never allocates.
#include "core/oversampling/Oversampler.h"
#include <algorithm>
#include <cmath>

namespace mm
{
class TruePeakMeter
{
public:
    static constexpr float kFloorDbtp = -200.0f;

    void prepare(double sampleRate, int channels, int maxBlock)
    {
        numChannels = std::max(1, channels);
        blockSize = std::max(1, maxBlock);
        factor = sampleRate >= 96000.0 ? 2 : 4;
        os.prepare(sampleRate, blockSize, numChannels, factor);
        work.setSize(numChannels, blockSize, false, false, true);
        reset();
    }

    // returns this block's maximum true-peak over channels in dBTP, and holds the maximum since reset
    float processBlock(const float* const* in, int n) noexcept
    {
        float blockMax = kFloorDbtp;
        for (int pos = 0; pos < n; pos += blockSize)
        {
            const int count = std::min(blockSize, n - pos);
            for (int ch = 0; ch < numChannels; ++ch)
                work.copyFrom(ch, 0, in[ch] + pos, count);
            juce::dsp::AudioBlock<float> block(work.getArrayOfWritePointers(), static_cast<size_t>(numChannels), 0, static_cast<size_t>(count));
            auto up = os.up(block);
            float peak = 0.0f;
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                const float* p = up.getChannelPointer(ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    peak = std::max(peak, std::abs(p[i]));
            }
            const float dbtp = peak > 0.0f ? 20.0f * std::log10(peak) : kFloorDbtp;
            blockMax = std::max(blockMax, dbtp);
        }
        held = std::max(held, blockMax);
        return blockMax;
    }

    float maxDbtp() const noexcept { return held; }
    int oversamplingFactor() const noexcept { return factor; }
    void reset() noexcept
    {
        os.reset();
        held = kFloorDbtp;
    }

private:
    Oversampler os;
    juce::AudioBuffer<float> work;
    int numChannels = 2, blockSize = 512, factor = 4;
    float held = kFloorDbtp;
};
} // namespace mm
