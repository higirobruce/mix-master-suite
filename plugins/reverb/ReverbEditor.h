#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ReverbProcessor;

// Placeholder until Task 8 (decay display).
class ReverbEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit ReverbEditor(ReverbProcessor& p);
};
