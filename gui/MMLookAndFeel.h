#pragma once
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::gui
{
class MMLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit MMLookAndFeel(Theme theme);
    void setTheme(Theme theme);
    const Theme& theme() const noexcept { return currentTheme; }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;
    juce::Font regular(float size) const;
    juce::Font medium(float size) const;

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW,
                      int buttonH, juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;

private:
    void applyColours();
    Theme currentTheme;
    juce::Typeface::Ptr regularFace, mediumFace;
};
} // namespace mm::gui
