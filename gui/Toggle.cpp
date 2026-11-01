#include "gui/Toggle.h"

namespace mm::gui
{
Toggle::Toggle(juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& text)
    : id(paramId), toggle(text)
{
    toggle.setClickingTogglesState(true);
    toggle.setTitle(text);
    if (apvts.getParameter(paramId) != nullptr)
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramId, toggle);
    addAndMakeVisible(toggle);
}
} // namespace mm::gui
