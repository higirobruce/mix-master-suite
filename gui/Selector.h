#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <string>

namespace mm::gui
{
// Combo box attached to a choice parameter.
class Selector : public juce::Component
{
public:
    Selector(juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& label);
    const std::string& paramId() const noexcept { return id; }
    void resized() override;

private:
    std::string id;
    juce::ComboBox box;
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
} // namespace mm::gui
