#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class UtilityProcessor;

// Placeholder until the shared chrome lands (M0 Task 9): JUCE's generic parameter editor.
class UtilityEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit UtilityEditor(UtilityProcessor& p);
};
