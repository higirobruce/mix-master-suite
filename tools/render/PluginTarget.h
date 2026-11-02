#pragma once
#include "tools/render/Target.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace mmr
{
// Fixed-tempo play head so tempo-synced plugins can be measured (job key "bpm").
class FixedTempoPlayHead : public juce::AudioPlayHead
{
public:
    void set(double newBpm) noexcept { bpm = newBpm; }
    void advance(int samples) noexcept { position += samples; }
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setBpm(bpm);
        info.setIsPlaying(true);
        info.setTimeInSamples(position);
        return info;
    }

private:
    double bpm = 120.0;
    juce::int64 position = 0;
};

// Hosts a VST3 (or AU) bundle through JUCE's plugin format manager.
class PluginTarget : public Target
{
public:
    static std::unique_ptr<PluginTarget> load(const juce::String& path, juce::String& error);

    bool prepare(double sampleRate, int maxBlock, const Job& job, juce::String& error) override;
    void release() override;
    bool setParam(const juce::String& id, double realValue, const juce::var& ranges, juce::String& error) override;
    void process(juce::AudioBuffer<float>& buffer, bool hostBypass) override;
    int latencySamples() const override { return instance != nullptr ? instance->getLatencySamples() : 0; }
    juce::var paramsDump() const override;
    bool stateRoundtrip(juce::String& error) override;

private:
    juce::AudioProcessorParameter* findParam(const juce::String& id) const;
    bool createInstance(juce::String& error);

    juce::AudioPluginFormatManager formats;
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    double preparedRate = 48000.0;
    int preparedBlock = 64;
    Job preparedJob;
    juce::MidiBuffer midi;
    FixedTempoPlayHead playHead;
};
} // namespace mmr
