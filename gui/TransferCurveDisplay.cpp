#include "gui/TransferCurveDisplay.h"

namespace mm::gui
{
TransferCurveDisplay::TransferCurveDisplay(GainFn gain, float lo, float hi) : gainFn(std::move(gain)), minDb(lo), maxDb(hi)
{
    setInterceptsMouseClicks(false, false);
}

void TransferCurveDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

void TransferCurveDisplay::setLive(float inputDb, float gainReductionDb)
{
    const float in = juce::jlimit(minDb - 6.0f, maxDb + 6.0f, inputDb);
    if (std::abs(in - liveIn) < 0.1f && std::abs(gainReductionDb - liveGr) < 0.05f)
        return;
    liveIn = in;
    liveGr = gainReductionDb;
    repaint();
}

float TransferCurveDisplay::xForDb(float db) const noexcept
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * (db - minDb) / (maxDb - minDb);
}

float TransferCurveDisplay::yForDb(float db) const noexcept
{
    const auto a = plotArea();
    return a.getBottom() - a.getHeight() * (db - minDb) / (maxDb - minDb);
}

void TransferCurveDisplay::paint(juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), theme.cornerRadius);
    g.setColour(theme.grid);
    for (float db = minDb; db <= maxDb + 0.01f; db += 12.0f)
    {
        g.drawVerticalLine(juce::roundToInt(xForDb(db)), a.getY(), a.getBottom());
        g.drawHorizontalLine(juce::roundToInt(yForDb(db)), a.getX(), a.getRight());
    }
    // unity line
    g.setColour(theme.textDim.withAlpha(0.5f));
    g.drawLine(xForDb(minDb), yForDb(minDb), xForDb(maxDb), yForDb(maxDb), 1.0f);
    // curve
    juce::Path p;
    const int steps = juce::jmax(2, static_cast<int>(a.getWidth()));
    for (int i = 0; i <= steps; ++i)
    {
        const float in = minDb + (maxDb - minDb) * static_cast<float>(i) / static_cast<float>(steps);
        const float out = juce::jlimit(minDb, maxDb, outputDbFor(in));
        const juce::Point<float> pt(xForDb(in), yForDb(out));
        if (i == 0) p.startNewSubPath(pt);
        else p.lineTo(pt);
    }
    g.setColour(theme.accent);
    g.strokePath(p, juce::PathStrokeType(2.0f));
    // live dot
    if (liveIn > minDb - 5.0f)
    {
        const float out = juce::jlimit(minDb, maxDb, liveIn - liveGr);
        const float in = juce::jlimit(minDb, maxDb, liveIn);
        g.setColour(theme.text);
        g.fillEllipse(xForDb(in) - 4.0f, yForDb(out) - 4.0f, 8.0f, 8.0f);
    }
    g.setColour(theme.textDim);
    g.setFont(10.0f);
    g.drawText("IN dB", a.withTrimmedTop(a.getHeight() - 12.0f).withTrimmedLeft(a.getWidth() - 40.0f), juce::Justification::centredRight);
}
} // namespace mm::gui
