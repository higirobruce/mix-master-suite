#include "plugins/utility/UtilityEditor.h"
#include "plugins/utility/UtilityProcessor.h"

UtilityEditor::UtilityEditor(UtilityProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(400, 500); }
