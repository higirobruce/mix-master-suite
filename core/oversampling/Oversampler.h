#pragma once
// Oversampling wrapper around juce::dsp::Oversampling (FIR equiripple half-band, maximum quality, integer latency).
// Factor 1 is a passthrough with zero latency. Header-only so mm_core stays an interface library.
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace mm
{
class Oversampler
{
public:
    void prepare(double sampleRate, int maxBlock, int channels, int factor)
    {
        sr = sampleRate;
        fac = factor >= 8 ? 8 : (factor >= 4 ? 4 : (factor >= 2 ? 2 : 1));
        if (fac == 1)
        {
            impl.reset();
            return;
        }
        const size_t stages = fac == 8 ? 3 : (fac == 4 ? 2 : 1);
        impl = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(channels), stages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        impl->initProcessing(static_cast<size_t>(maxBlock));
    }

    juce::dsp::AudioBlock<float> up(juce::dsp::AudioBlock<float> in) noexcept
    {
        return impl ? impl->processSamplesUp(in) : in;
    }
    void down(juce::dsp::AudioBlock<float> out) noexcept
    {
        if (impl)
            impl->processSamplesDown(out);
    }

    int latencySamples() const noexcept { return impl ? static_cast<int>(impl->getLatencyInSamples() + 0.5f) : 0; }
    double oversampledRate() const noexcept { return sr * fac; }
    int factor() const noexcept { return fac; }
    void reset() noexcept
    {
        if (impl)
            impl->reset();
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> impl;
    double sr = 48000.0;
    int fac = 1;
};
} // namespace mm
