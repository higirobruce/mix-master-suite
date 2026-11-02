#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ImagerProcessor;

// Placeholder until Task 5 (goniometer + correlation bar).
class ImagerEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit ImagerEditor(ImagerProcessor& p);
};
