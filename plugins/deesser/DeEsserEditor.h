#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class DeEsserProcessor;

// Placeholder until Task 3 (sibilance history, listen).
class DeEsserEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit DeEsserEditor(DeEsserProcessor& p);
};
