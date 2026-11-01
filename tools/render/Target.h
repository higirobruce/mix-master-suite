#pragma once
#include "tools/render/Job.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace mmr
{
// What the render loop drives: a hosted plugin or a core module.
class Target
{
public:
    virtual ~Target() = default;
    virtual bool prepare(double sampleRate, int maxBlock, const Job& job, juce::String& error) = 0;
    virtual void release() = 0;
    virtual bool setParam(const juce::String& id, double realValue, const juce::var& ranges, juce::String& error) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer, bool hostBypass) = 0;
    virtual int latencySamples() const = 0;
    virtual juce::var paramsDump() const = 0;
    virtual bool stateRoundtrip(juce::String& error) = 0; // save state, fresh instance, load
};
} // namespace mmr
