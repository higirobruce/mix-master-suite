#include "plugins/delay/DelayEditor.h"
#include "plugins/delay/DelayProcessor.h"

DelayEditor::DelayEditor(DelayProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(500, 600); }
