#include "gui/HarmonicDisplay.h"

namespace mm::gui
{
HarmonicDisplay::HarmonicDisplay(int numHarmonics, float lo) : levels(static_cast<size_t>(juce::jmax(1, numHarmonics)), lo), minDb(lo)
{
    setInterceptsMouseClicks(false, false);
}

void HarmonicDisplay::setLevels(const float* levelsDb, int count)
{
    bool changed = false;
    for (int i = 0; i < juce::jmin(count, numBars()); ++i)
    {
        const float v = juce::jlimit(minDb, 12.0f, levelsDb[i]);
        if (std::abs(v - levels[static_cast<size_t>(i)]) > 0.05f)
        {
            levels[static_cast<size_t>(i)] = v;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void HarmonicDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

void HarmonicDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, theme.cornerRadius);
    auto plot = bounds.reduced(10.0f, 8.0f).withTrimmedBottom(16.0f);
    const float range = -minDb;
    auto yFor = [&](float db) { return plot.getBottom() - plot.getHeight() * juce::jlimit(0.0f, 1.0f, (db - minDb) / range); };
    g.setColour(theme.grid);
    for (float db = minDb; db <= 0.0f; db += 18.0f)
        g.drawHorizontalLine(juce::roundToInt(yFor(db)), plot.getX(), plot.getRight());
    const int n = numBars();
    const float slot = plot.getWidth() / static_cast<float>(n);
    const float barW = juce::jmax(4.0f, slot * 0.6f);
    g.setFont(10.0f);
    for (int i = 0; i < n; ++i)
    {
        const float x = plot.getX() + slot * (static_cast<float>(i) + 0.5f);
        const float top = yFor(levels[static_cast<size_t>(i)]);
        const bool even = (i + 1) % 2 == 0;
        g.setColour(even ? theme.accent : theme.text.withAlpha(0.85f));
        if (levels[static_cast<size_t>(i)] > minDb + 0.5f)
            g.fillRoundedRectangle(x - barW * 0.5f, top, barW, plot.getBottom() - top, 2.0f);
        g.setColour(theme.textDim);
        g.drawText(juce::String(i + 1), juce::Rectangle<float>(x - slot * 0.5f, plot.getBottom() + 2.0f, slot, 14.0f), juce::Justification::centred);
    }
    g.setColour(theme.textDim);
    g.drawText("even = accent, odd = white", bounds.reduced(10.0f, 6.0f), juce::Justification::topRight);
}
} // namespace mm::gui
