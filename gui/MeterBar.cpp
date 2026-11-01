#include "gui/MeterBar.h"

namespace mm::gui
{
void MeterBar::setLevels(float peakDb, float rmsDb)
{
    if (std::abs(peakDb - peak) < 0.1f && std::abs(rmsDb - rms) < 0.1f)
        return;
    peak = peakDb;
    rms = rmsDb;
    peakHold = juce::jmax(peakHold - 0.5f, peak);
    if (peak >= 0.0f)
        clipped = true;
    repaint();
}

float MeterBar::dbToY(float db, float height) noexcept
{
    const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
    return height * (1.0f - norm);
}

void MeterBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float h = bounds.getHeight();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, 2.0f);
    // segments: low (<-18), mid (<-6), high
    auto fillTo = [&](float topDb, float bottomDb, juce::Colour c) {
        const float top = juce::jmax(dbToY(juce::jmin(rms, topDb), h), 0.0f);
        const float bottom = dbToY(bottomDb, h);
        if (rms > bottomDb)
        {
            g.setColour(c);
            g.fillRect(bounds.getX() + 1.0f, top, bounds.getWidth() - 2.0f, bottom - top);
        }
    };
    fillTo(-18.0f, -60.0f, theme.meterLow);
    fillTo(-6.0f, -18.0f, theme.meterMid);
    fillTo(6.0f, -6.0f, theme.meterHigh);
    g.setColour(theme.text);
    const float py = dbToY(peakHold, h);
    g.fillRect(bounds.getX() + 1.0f, py, bounds.getWidth() - 2.0f, 1.5f);
    if (clipped)
    {
        g.setColour(theme.meterHigh);
        g.fillRect(bounds.getX(), 0.0f, bounds.getWidth(), 3.0f);
    }
    g.setColour(theme.grid);
    for (float db : {0.0f, -6.0f, -18.0f, -36.0f})
        g.fillRect(bounds.getX(), dbToY(db, h), bounds.getWidth(), 0.5f);
}
} // namespace mm::gui
