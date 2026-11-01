#include "plugins/gate/GateEditor.h"
#include "plugins/gate/GateProcessor.h"

GateEditor::GateEditor(GateProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(500, 600); }
