#include "plugins/deesser/DeEsserEditor.h"
#include "plugins/deesser/DeEsserProcessor.h"

DeEsserEditor::DeEsserEditor(DeEsserProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(500, 500); }
