#include "gui/NeedleMeter.h"
#include <cmath>
#include <numbers>

namespace mm::gui
{
void NeedleMeter::advance(double seconds) noexcept
{
    const float a = static_cast<float>(1.0 - std::exp(-seconds / kTauSeconds));
    shown += (target - shown) * a;
    repaint();
}

void NeedleMeter::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(6.0f);
    // the dial: a cream face with a dark rim, whatever the theme (the analogue skin of the mockup)
    const juce::Colour face(0xfff0e9d8), ink(0xff2a2a2a), red(0xffb03a2e);
    g.setColour(face);
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(theme.accent.withAlpha(0.6f));
    g.drawRoundedRectangle(area, 10.0f, 1.5f);
    const float cx = area.getCentreX(), cy = area.getBottom() - area.getHeight() * 0.15f;
    const float radius = std::min(area.getWidth() * 0.46f, area.getHeight() * 0.78f);
    // scale: rest (0 dB) at +40 degrees from vertical, full range at -40
    auto angleFor = [&](float db) { return juce::degreesToRadians(40.0f - 80.0f * juce::jlimit(0.0f, 1.0f, -db / range)); };
    g.setColour(ink);
    for (int tick = 0; tick <= 4; ++tick)
    {
        const float db = -range * static_cast<float>(tick) / 4.0f;
        const float a = angleFor(db);
        const juce::Point<float> p1(cx + radius * std::sin(a), cy - radius * std::cos(a));
        const juce::Point<float> p2(cx + (radius - 10.0f) * std::sin(a), cy - (radius - 10.0f) * std::cos(a));
        g.drawLine({p1, p2}, tick == 0 ? 2.0f : 1.2f);
        g.setFont(12.0f);
        const juce::Point<float> t(cx + (radius - 24.0f) * std::sin(a), cy - (radius - 24.0f) * std::cos(a));
        g.drawText(juce::String(static_cast<int>(db)), juce::Rectangle<float>(t.x - 16.0f, t.y - 8.0f, 32.0f, 16.0f), juce::Justification::centred);
    }
    juce::Path arc;
    arc.addCentredArc(cx, cy, radius, radius, 0.0f, angleFor(-range), angleFor(0.0f), true);
    g.setColour(ink.withAlpha(0.5f));
    g.strokePath(arc, juce::PathStrokeType(1.0f));
    const float a = angleFor(shown);
    g.setColour(red);
    g.drawLine({cx, cy, cx + radius * 0.96f * std::sin(a), cy - radius * 0.96f * std::cos(a)}, 2.5f);
    g.setColour(ink);
    g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
    g.setFont(13.0f);
    g.drawText("GAIN REDUCTION  " + juce::String(-shown, 1) + " dB", area.withTrimmedTop(area.getHeight() - 22.0f).reduced(8.0f, 2.0f),
               juce::Justification::centredRight);
}
} // namespace mm::gui
