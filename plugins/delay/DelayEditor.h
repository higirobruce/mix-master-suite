#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class DelayProcessor;

// Placeholder until Task 5 (tap display with tempo grid).
class DelayEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit DelayEditor(DelayProcessor& p);
};
