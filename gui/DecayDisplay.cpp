#include "gui/DecayDisplay.h"
#include <cmath>

namespace mm::gui
{
DecayDisplay::DecayDisplay() : history(300, -120.0f) {}

void DecayDisplay::setDecay(double seconds, double pre, float earlyShare)
{
    if (std::abs(seconds - decay) < 1e-4 && std::abs(pre - predelayMs) < 0.01 && std::abs(earlyShare - early) < 1e-3)
        return;
    decay = juce::jmax(0.05, seconds);
    predelayMs = pre;
    early = earlyShare;
    repaint();
}

void DecayDisplay::pushLevel(float wetDb)
{
    history[static_cast<size_t>(head)] = wetDb;
    head = (head + 1) % static_cast<int>(history.size());
    count = juce::jmin(count + 1, static_cast<int>(history.size()));
    repaint();
}

void DecayDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

float DecayDisplay::xForSeconds(double s) const noexcept
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * static_cast<float>(juce::jlimit(0.0, 1.0, s / axisSeconds()));
}

double DecayDisplay::secondsForX(float x) const noexcept
{
    const auto a = plotArea();
    return juce::jlimit(0.05, 20.0, static_cast<double>((x - a.getX()) / a.getWidth()) * axisSeconds());
}

float DecayDisplay::envelopeDb(double t) const noexcept { return static_cast<float>(-60.0 * juce::jmax(0.0, t) / decay); }

void DecayDisplay::dragDecayTo(float x)
{
    const double s = secondsForX(x) - predelayMs * 1e-3; // the handle is drawn at decay + pre-delay
    decay = juce::jmax(0.05, s);
    if (onDecayChange)
        onDecayChange(decay);
    repaint();
}

void DecayDisplay::mouseDown(const juce::MouseEvent& e)
{
    dragAxis = axisSeconds();
    dragging = true;
    if (onGesture)
        onGesture(true);
    dragDecayTo(e.position.x);
}

void DecayDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging)
        dragDecayTo(e.position.x);
}

void DecayDisplay::mouseUp(const juce::MouseEvent&)
{
    if (dragging && onGesture)
        onGesture(false);
    dragging = false;
}

void DecayDisplay::paint(juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), theme.cornerRadius);
    auto yFor = [&](float db) { return a.getY() + a.getHeight() * juce::jlimit(0.0f, 1.0f, -db / 72.0f); };
    g.setFont(10.0f);
    for (float db = 0.0f; db >= -60.0f; db -= 12.0f)
    {
        g.setColour(theme.grid);
        g.drawHorizontalLine(juce::roundToInt(yFor(db)), a.getX(), a.getRight());
        g.setColour(theme.textDim);
        g.drawText(juce::String(static_cast<int>(db)), juce::Rectangle<float>(a.getX(), yFor(db) - 12.0f, 30.0f, 12.0f), juce::Justification::left);
    }
    // time ticks
    const double axis = axisSeconds();
    const double tick = axis > 8.0 ? 2.0 : (axis > 3.0 ? 1.0 : (axis > 1.5 ? 0.5 : 0.25));
    for (double t = tick; t < axis; t += tick)
    {
        const float x = xForSeconds(t);
        g.setColour(theme.grid);
        g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
        g.setColour(theme.textDim);
        g.drawText(juce::String(t, 2) + " s", juce::Rectangle<float>(x - 24.0f, a.getBottom() - 12.0f, 48.0f, 12.0f), juce::Justification::centred);
    }
    // early reflections block (pre-delay to pre-delay + 80 ms) shaded by the early share
    const float x0 = xForSeconds(predelayMs * 1e-3), x1 = xForSeconds(predelayMs * 1e-3 + 0.08);
    g.setColour(theme.accent.withAlpha(0.12f + 0.3f * early));
    g.fillRect(x0, a.getY(), juce::jmax(2.0f, x1 - x0), a.getHeight());
    // analytic late envelope from the pre-delay
    juce::Path env;
    const int steps = juce::jmax(2, static_cast<int>(a.getWidth()));
    for (int i = 0; i <= steps; ++i)
    {
        const double t = axis * i / steps;
        const float db = t < predelayMs * 1e-3 ? -72.0f : envelopeDb(t - predelayMs * 1e-3);
        const juce::Point<float> p(xForSeconds(t), yFor(db));
        if (i == 0) env.startNewSubPath(p);
        else env.lineTo(p);
    }
    g.setColour(theme.accent);
    g.strokePath(env, juce::PathStrokeType(2.0f));
    // the draggable -60 dB point
    const float xd = xForSeconds(decay + predelayMs * 1e-3);
    g.fillEllipse(xd - 5.0f, yFor(-60.0f) - 5.0f, 10.0f, 10.0f);
    g.setColour(theme.text);
    g.drawText("RT60 " + juce::String(decay, 2) + " s", juce::Rectangle<float>(xd - 60.0f, yFor(-60.0f) - 24.0f, 120.0f, 14.0f), juce::Justification::centred);
    // live wet level history (right-aligned, newest at the right)
    if (count > 1)
    {
        juce::Path hist;
        const int cap = static_cast<int>(history.size());
        const float dx = a.getWidth() / static_cast<float>(cap - 1);
        for (int i = 0; i < count; ++i)
        {
            const float v = history[static_cast<size_t>(((head - count + i) % cap + cap) % cap)];
            const juce::Point<float> p(a.getRight() - dx * static_cast<float>(count - 1 - i), yFor(v));
            if (i == 0) hist.startNewSubPath(p);
            else hist.lineTo(p);
        }
        g.setColour(theme.textDim.withAlpha(0.7f));
        g.strokePath(hist, juce::PathStrokeType(1.0f));
    }
}
} // namespace mm::gui
