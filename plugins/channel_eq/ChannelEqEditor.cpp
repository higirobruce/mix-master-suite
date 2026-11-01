#include "plugins/channel_eq/ChannelEqEditor.h"
#include "plugins/channel_eq/ChannelEqProcessor.h"

ChannelEqEditor::ChannelEqEditor(ChannelEqProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(500, 700); }
