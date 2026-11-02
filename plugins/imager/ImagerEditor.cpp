#include "plugins/imager/ImagerEditor.h"
#include "plugins/imager/ImagerProcessor.h"

ImagerEditor::ImagerEditor(ImagerProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(500, 500); }
