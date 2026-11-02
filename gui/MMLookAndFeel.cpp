#include "gui/MMLookAndFeel.h"
#include "mm_gui_fonts.h"

namespace mm::gui
{
MMLookAndFeel::MMLookAndFeel(Theme t) : currentTheme(t)
{
    regularFace = juce::Typeface::createSystemTypefaceFor(mm_gui_fonts::InterRegular_ttf, mm_gui_fonts::InterRegular_ttfSize);
    mediumFace = juce::Typeface::createSystemTypefaceFor(mm_gui_fonts::InterMedium_ttf, mm_gui_fonts::InterMedium_ttfSize);
    if (regularFace != nullptr)
        setDefaultSansSerifTypeface(regularFace);
    applyColours();
}

void MMLookAndFeel::setTheme(Theme t)
{
    currentTheme = t;
    applyColours();
}

void MMLookAndFeel::applyColours()
{
    const auto& t = currentTheme;
    setColour(juce::ResizableWindow::backgroundColourId, t.bg);
    setColour(juce::Label::textColourId, t.text);
    setColour(juce::Slider::rotarySliderFillColourId, t.accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, t.grid);
    setColour(juce::Slider::textBoxTextColourId, t.text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, t.accent.withAlpha(0.4f));
    setColour(juce::TextButton::buttonColourId, t.panelRaised);
    setColour(juce::TextButton::buttonOnColourId, t.accent);
    setColour(juce::TextButton::textColourOffId, t.text);
    setColour(juce::TextButton::textColourOnId, t.isDark ? juce::Colour(0xff141518) : juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, t.panelRaised);
    setColour(juce::ComboBox::textColourId, t.text);
    setColour(juce::ComboBox::outlineColourId, t.outline);
    setColour(juce::ComboBox::arrowColourId, t.textDim);
    setColour(juce::PopupMenu::backgroundColourId, t.panelRaised);
    setColour(juce::PopupMenu::textColourId, t.text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, t.text);
    setColour(juce::TextEditor::textColourId, t.text);
    setColour(juce::TextEditor::highlightColourId, t.accent.withAlpha(0.4f));
    setColour(juce::CaretComponent::caretColourId, t.accent);
}

juce::Typeface::Ptr MMLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    if (font.isBold() && mediumFace != nullptr)
        return mediumFace;
    if (regularFace != nullptr)
        return regularFace;
    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

juce::Font MMLookAndFeel::regular(float size) const
{
    return regularFace != nullptr ? juce::Font(juce::FontOptions(regularFace).withHeight(size))
                                  : juce::Font(juce::FontOptions(size));
}

juce::Font MMLookAndFeel::medium(float size) const
{
    return mediumFace != nullptr ? juce::Font(juce::FontOptions(mediumFace).withHeight(size))
                                 : juce::Font(juce::FontOptions(size, juce::Font::bold));
}

void MMLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
                                     float startAngle, float endAngle, juce::Slider& slider)
{
    const auto& t = currentTheme;
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = juce::jmax(2.0f, radius * 0.12f);
    const float arcRadius = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(t.grid);
    g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value sweep from the parameter's "zero" (centre for bipolar ranges, start otherwise)
    const auto range = slider.getRange();
    const bool bipolar = range.getStart() < 0.0 && range.getEnd() > 0.0;
    const float zeroAngle = bipolar ? startAngle + static_cast<float>(slider.valueToProportionOfLength(0.0)) * (endAngle - startAngle)
                                    : startAngle;
    juce::Path sweep;
    sweep.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin(zeroAngle, angle),
                        juce::jmax(zeroAngle, angle), true);
    g.setColour(slider.isEnabled() ? t.accent : t.textDim);
    g.strokePath(sweep, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    const float innerR = arcRadius - lineW * 1.6f;
    juce::Point<float> tip(centre.x + innerR * std::sin(angle), centre.y - innerR * std::cos(angle));
    juce::Point<float> root(centre.x + innerR * 0.55f * std::sin(angle), centre.y - innerR * 0.55f * std::cos(angle));
    g.setColour(t.text);
    g.drawLine({root, tip}, lineW * 0.8f);
}

void MMLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto& t = currentTheme;
    auto bounds = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    juce::Colour fill = on ? t.accent : t.panelRaised;
    if (down)
        fill = fill.darker(0.2f);
    else if (over)
        fill = fill.brighter(0.08f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, t.cornerRadius);
    g.setColour(on ? t.accent : t.outline);
    g.drawRoundedRectangle(bounds, t.cornerRadius, 1.0f);
}

juce::Font MMLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return medium(juce::jmin(13.0f, static_cast<float>(buttonHeight) * 0.6f));
}

void MMLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto& t = currentTheme;
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(t.panelRaised);
    g.fillRoundedRectangle(bounds, t.cornerRadius);
    g.setColour(t.outline);
    g.drawRoundedRectangle(bounds, t.cornerRadius, 1.0f);
    juce::Path arrow;
    const float ax = static_cast<float>(width) - 14.0f, ay = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour(box.isEnabled() ? t.textDim : t.grid);
    g.fillPath(arrow);
}

juce::Font MMLookAndFeel::getLabelFont(juce::Label& l)
{
    // slider readouts shrink with their box so values never truncate in narrow columns
    if (auto* slider = dynamic_cast<juce::Slider*>(l.getParentComponent()))
    {
        const float h = juce::jlimit(9.0f, 13.0f, static_cast<float>(slider->getTextBoxWidth()) / 5.0f);
        return regular(h);
    }
    return regular(l.getFont().getHeight());
}

void MMLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour(currentTheme.panel);
    g.fillRoundedRectangle(juce::Rectangle<int>(0, 0, width, height).toFloat(), currentTheme.cornerRadius);
}

void MMLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& e)
{
    g.setColour(e.hasKeyboardFocus(true) ? currentTheme.accent : currentTheme.outline);
    g.drawRoundedRectangle(juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f), currentTheme.cornerRadius, 1.0f);
}
} // namespace mm::gui
