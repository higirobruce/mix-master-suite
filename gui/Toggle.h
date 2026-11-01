#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <string>

namespace mm::gui
{
// Flat toggle button attached to a bool parameter.
class Toggle : public juce::Component
{
public:
    Toggle(juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& text);
    const std::string& paramId() const noexcept { return id; }
    juce::TextButton& button() noexcept { return toggle; }
    void resized() override { toggle.setBounds(getLocalBounds()); }

private:
    std::string id;
    juce::TextButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
} // namespace mm::gui
