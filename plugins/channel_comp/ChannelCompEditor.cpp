#include "plugins/channel_comp/ChannelCompEditor.h"
#include "plugins/channel_comp/ChannelCompProcessor.h"

ChannelCompEditor::ChannelCompEditor(ChannelCompProcessor& p) : juce::GenericAudioProcessorEditor(p) { setSize(500, 600); }
