#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace mmr
{
// Builds the input signal from the job's "input" spec. {"file": path} loads a WAV; {"gen": ...} synthesises.
bool makeInput(const juce::var& spec, double sampleRate, int channels, juce::AudioBuffer<float>& out,
               juce::String& error);
bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate);
} // namespace mmr
