#include "plugins/reverb/ReverbEditor.h"
#include "plugins/reverb/ReverbProcessor.h"

ReverbEditor::ReverbEditor(ReverbProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(500, 600); }
