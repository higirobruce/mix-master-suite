#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ChannelCompProcessor;

// Placeholder until Task 5 (transfer curve + GR history editor).
class ChannelCompEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit ChannelCompEditor(ChannelCompProcessor& p);
};
