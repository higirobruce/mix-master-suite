#include "gui/Selector.h"

namespace mm::gui
{
Selector::Selector(juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& label) : id(paramId)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
    {
        box.addItemList(p->choices, 1);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId, box);
    }
    box.setTitle(label);
    caption.setText(label, juce::dontSendNotification);
    caption.setJustificationType(juce::Justification::centred);
    caption.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(caption);
    addAndMakeVisible(box);
}

void Selector::resized()
{
    auto area = getLocalBounds();
    caption.setBounds(area.removeFromTop(14));
    box.setBounds(area.reduced(0, 2));
}
} // namespace mm::gui
