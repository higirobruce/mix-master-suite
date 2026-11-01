#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ChannelEqProcessor;

// Placeholder until Task 3 (curve + spectrum editor).
class ChannelEqEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit ChannelEqEditor(ChannelEqProcessor& p);
};
