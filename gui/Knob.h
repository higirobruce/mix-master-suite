#pragma once
#include "plugin_base/ParamTable.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <string>

namespace mm::gui
{
// Flat-arc rotary knob attached to an APVTS parameter. Label under the knob, value readout editable on click,
// double-click resets to default, shift-drag for fine control, alt-click opens the value for typing.
class Knob : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& label);
    const std::string& paramId() const noexcept { return id; }
    juce::Slider& slider() noexcept { return rotary; }
    void resized() override;
    void setLabelText(const juce::String& text) { caption.setText(text, juce::dontSendNotification); }

private:
    void mouseDown(const juce::MouseEvent&) override;
    std::string id;
    juce::Slider rotary;
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
} // namespace mm::gui
