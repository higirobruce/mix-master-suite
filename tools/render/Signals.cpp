#include "tools/render/Signals.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <numbers>
#include <random>

namespace mmr
{
bool makeInput(const juce::var& spec, double sr, int channels, juce::AudioBuffer<float>& out, juce::String& error)
{
    if (spec.hasProperty("file"))
    {
        juce::WavAudioFormat wav;
        juce::File f(spec["file"].toString());
        std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(f.createInputStream().release(), true));
        if (r == nullptr)
        {
            error = "cannot read input wav " + f.getFullPathName();
            return false;
        }
        juce::AudioBuffer<float> tmp(static_cast<int>(r->numChannels), static_cast<int>(r->lengthInSamples));
        r->read(&tmp, 0, tmp.getNumSamples(), 0, true, true);
        out.setSize(channels, tmp.getNumSamples());
        for (int ch = 0; ch < channels; ++ch)
            out.copyFrom(ch, 0, tmp, juce::jmin(ch, tmp.getNumChannels() - 1), 0, tmp.getNumSamples());
        return true;
    }
    const juce::String gen = spec.getProperty("gen", "silence").toString();
    const double dur = static_cast<double>(spec.getProperty("dur_s", 1.0));
    const double freq = static_cast<double>(spec.getProperty("freq_hz", 1000.0));
    const double amp = std::pow(10.0, static_cast<double>(spec.getProperty("amp_db", 0.0)) / 20.0);
    const int n = static_cast<int>(std::lround(dur * sr));
    out.setSize(channels, n);
    out.clear();
    if (gen == "sine")
    {
        for (int i = 0; i < n; ++i)
        {
            const float v = static_cast<float>(amp * std::sin(2.0 * std::numbers::pi * freq * i / sr));
            for (int ch = 0; ch < channels; ++ch)
                out.setSample(ch, i, v);
        }
    }
    else if (gen == "impulse")
    {
        for (int ch = 0; ch < channels; ++ch)
            out.setSample(ch, 0, static_cast<float>(amp));
    }
    else if (gen == "dc")
    {
        for (int ch = 0; ch < channels; ++ch)
            juce::FloatVectorOperations::fill(out.getWritePointer(ch), static_cast<float>(amp), n);
    }
    else if (gen == "step")
    {
        const int at = static_cast<int>(std::lround(static_cast<double>(spec.getProperty("step_at_s", 0.0)) * sr));
        for (int ch = 0; ch < channels; ++ch)
            for (int i = at; i < n; ++i)
                out.setSample(ch, i, static_cast<float>(amp));
    }
    else if (gen == "noise")
    {
        std::mt19937 rng(static_cast<unsigned>(static_cast<int>(spec.getProperty("seed", 0))));
        std::uniform_real_distribution<float> d(-1.0f, 1.0f);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < n; ++i)
                out.setSample(ch, i, static_cast<float>(amp) * d(rng));
    }
    else if (gen == "burst")
    {
        const double on = static_cast<double>(spec.getProperty("burst_on_s", 0.5));
        const double off = static_cast<double>(spec.getProperty("burst_off_s", 0.5));
        const double period = on + off;
        for (int i = 0; i < n; ++i)
        {
            const double t = std::fmod(i / sr, period);
            const float v = t < on ? static_cast<float>(amp * std::sin(2.0 * std::numbers::pi * freq * i / sr)) : 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                out.setSample(ch, i, v);
        }
    }
    else if (gen != "silence")
    {
        error = "unknown generator " + gen;
        return false;
    }
    return true;
}

bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = file.createOutputStream();
    if (os == nullptr)
        return false;
    auto w = wav.createWriterFor(os, juce::AudioFormatWriterOptions()
                                         .withSampleRate(sampleRate)
                                         .withNumChannels(buffer.getNumChannels())
                                         .withBitsPerSample(32));
    if (w == nullptr)
        return false;
    return w->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}
} // namespace mmr
