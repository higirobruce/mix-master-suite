#pragma once
// Spectrum analyser: the audio thread pushes samples into a lock-free ring (per channel); the GUI thread pulls
// Hann-windowed FFT frames with 50 % overlap, converts to dBFS (0 dB = full-scale sine) and applies a peak hold
// with 20 dB/s decay. No allocation after prepare(); push() never blocks.
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace mm
{
class SpectrumAnalyser
{
public:
    void prepare(double sampleRate, int fftOrder, int channels)
    {
        sr = sampleRate;
        order = fftOrder;
        size = 1 << order;
        hop = size / 2;
        numChannels = std::max(1, channels);
        capacity = size * 4;
        ring.assign(static_cast<size_t>(capacity) * static_cast<size_t>(numChannels), 0.0f);
        write.store(0);
        read.store(0);
        fft = std::make_unique<juce::dsp::FFT>(order);
        window.resize(static_cast<size_t>(size));
        for (int i = 0; i < size; ++i)
            window[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(size));
        frame.assign(static_cast<size_t>(size) * 2, 0.0f);
        hold.assign(static_cast<size_t>(size / 2 + 1), -200.0f);
        decayPerFrame = 20.0f * static_cast<float>(hop) / static_cast<float>(sr);
    }

    // Audio thread: all channels of one block in a single call, one free-space check, one commit, so the channels
    // can never desynchronise. Returns false (and drops the whole block) if the ring is full.
    bool push(const float* const* channels, int numChannelsIn, int n) noexcept
    {
        const int w = write.load(std::memory_order_relaxed);
        const int r = read.load(std::memory_order_acquire);
        const int free = capacity - 1 - ((w - r + capacity) % capacity);
        if (n > free)
            return false;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* x = channels[std::min(ch, numChannelsIn - 1)];
            float* base = ring.data() + static_cast<size_t>(ch) * static_cast<size_t>(capacity);
            for (int i = 0; i < n; ++i)
                base[(w + i) % capacity] = x[i];
        }
        write.store((w + n) % capacity, std::memory_order_release);
        return true;
    }

    // GUI thread. Fills magDb (size/2 + 1 bins) when a new hop is available; channels are averaged.
    bool nextFrame(std::vector<float>& magDb)
    {
        const int w = write.load(std::memory_order_acquire);
        const int r = read.load(std::memory_order_relaxed);
        const int available = (w - r + capacity) % capacity;
        if (available < size)
            return false;
        for (int i = 0; i < size; ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                v += ring[static_cast<size_t>(ch) * static_cast<size_t>(capacity) + static_cast<size_t>((r + i) % capacity)];
            frame[static_cast<size_t>(i)] = v / static_cast<float>(numChannels) * window[static_cast<size_t>(i)];
        }
        for (size_t i = static_cast<size_t>(size); i < frame.size(); ++i)
            frame[i] = 0.0f;
        read.store((r + hop) % capacity, std::memory_order_release);
        fft->performFrequencyOnlyForwardTransform(frame.data(), true);
        magDb.resize(static_cast<size_t>(size / 2 + 1));
        const float norm = 4.0f / static_cast<float>(size); // Hann coherent gain 0.5, one-sided spectrum x2
        for (size_t k = 0; k < magDb.size(); ++k)
        {
            const float db = 20.0f * std::log10(frame[k] * norm + 1e-12f);
            hold[k] = std::max(hold[k] - decayPerFrame, db);
            magDb[k] = hold[k];
        }
        return true;
    }

    int fftSize() const noexcept { return size; }
    double binHz() const noexcept { return sr / size; }

private:
    double sr = 48000.0;
    int order = 12, size = 4096, hop = 2048, numChannels = 1, capacity = 16384;
    std::vector<float> ring, window, frame, hold;
    std::atomic<int> write{0}, read{0};
    std::unique_ptr<juce::dsp::FFT> fft;
    float decayPerFrame = 0.0f;
};
} // namespace mm
