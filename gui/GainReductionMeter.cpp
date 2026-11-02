#include "gui/GainReductionMeter.h"

namespace mm::gui
{
void GainReductionMeter::setGainReduction(float db)
{
    db = juce::jmax(0.0f, db);
    hold = juce::jmax(hold - 0.3f, db);
    if (std::abs(db - gr) < 0.05f)
        return;
    gr = db;
    repaint();
}

void GainReductionMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(b, 2.0f);
    auto bar = b.reduced(1.0f, 1.0f).removeFromTop(18.0f);
    juce::ignoreUnused(bar);
    const float h = b.getHeight() - 2.0f;
    const float fill = h * juce::jlimit(0.0f, 1.0f, gr / range);
    g.setColour(theme.accent);
    g.fillRect(b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, fill);
    const float hy = b.getY() + 1.0f + h * juce::jlimit(0.0f, 1.0f, hold / range);
    g.setColour(theme.text);
    g.fillRect(b.getX() + 1.0f, hy - 1.0f, b.getWidth() - 2.0f, 2.0f);
    g.setColour(theme.grid);
    for (float db = 6.0f; db < range; db += 6.0f)
        g.drawHorizontalLine(juce::roundToInt(b.getY() + 1.0f + h * db / range), b.getX(), b.getRight());
}
} // namespace mm::gui
