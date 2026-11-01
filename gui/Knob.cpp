#include "gui/Knob.h"
#include "gui/Theme.h"

namespace mm::gui
{
Knob::Knob(juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& label) : id(paramId)
{
    rotary.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rotary.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    rotary.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    rotary.setVelocityModeParameters(1.0, 1, 0.0, true, juce::ModifierKeys::shiftModifier); // shift = fine
    rotary.setScrollWheelEnabled(true);
    rotary.setTitle(label);
    rotary.setName(label);
    if (auto* p = apvts.getParameter(paramId))
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
            rotary.setDoubleClickReturnValue(true, ranged->convertFrom0to1(ranged->getDefaultValue()));
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, rotary);
    }
    caption.setText(label, juce::dontSendNotification);
    caption.setJustificationType(juce::Justification::centred);
    caption.setFont(juce::FontOptions(11.0f));
    caption.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(rotary);
    addAndMakeVisible(caption);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    caption.setBounds(area.removeFromTop(14));
    rotary.setBounds(area);
}

void Knob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isAltDown())
        rotary.showTextBox();
}
} // namespace mm::gui
