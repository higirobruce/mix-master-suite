#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class GateProcessor;

// Placeholder until Task 7 (history with open/closed shading).
class GateEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit GateEditor(GateProcessor& p);
};
