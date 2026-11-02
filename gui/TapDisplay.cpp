#include "gui/TapDisplay.h"
#include "core/delay/TempoSync.h"
#include <cmath>

namespace mm::gui
{
TapDisplay::TapDisplay() {}

void TapDisplay::setTaps(double msL, double msR, float feedback, bool pp, bool sync)
{
    if (std::abs(msL - timeMs[0]) < 0.01 && std::abs(msR - timeMs[1]) < 0.01 && std::abs(feedback - fb) < 1e-3 && pp == pingPong && sync == synced)
        return;
    timeMs[0] = msL;
    timeMs[1] = msR;
    fb = feedback;
    pingPong = pp;
    synced = sync;
    repaint();
}

void TapDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

// square-root time axis: short slapbacks stay readable, 4 s still fits
float TapDisplay::xForMs(double ms) const noexcept
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * static_cast<float>(std::sqrt(juce::jlimit(0.0, kMaxMs, ms) / kMaxMs));
}

double TapDisplay::msForX(float x) const noexcept
{
    const auto a = plotArea();
    const double u = juce::jlimit(0.0, 1.0, static_cast<double>((x - a.getX()) / a.getWidth()));
    return juce::jlimit(1.0, kMaxMs, u * u * kMaxMs);
}

double TapDisplay::snap(double ms, juce::String& label) const
{
    label.clear();
    if (synced || tempo <= 0.0)
        return ms;
    static const int divisions[] = {1, 2, 4, 8, 16, 32};
    double best = ms, bestErr = 0.05;
    for (int d : divisions)
        for (int kind = 0; kind < 3; ++kind)
        {
            const double note = mm::TempoSync::noteMs(tempo, d, kind == 1, kind == 2);
            if (note <= 0.0 || note > kMaxMs)
                continue;
            const double err = std::abs(ms - note) / note;
            if (err < bestErr)
            {
                bestErr = err;
                best = note;
                label = "1/" + juce::String(d) + (kind == 1 ? " d" : (kind == 2 ? " t" : ""));
            }
        }
    return best;
}

void TapDisplay::dragTap(int channel, float x)
{
    channel = juce::jlimit(0, 1, channel);
    const double ms = snap(msForX(x), snapLabel);
    timeMs[channel] = ms;
    if (onTimeChange)
        onTimeChange(channel, ms);
    repaint();
}

void TapDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (synced)
        return;
    const auto a = plotArea();
    dragging = e.position.y < a.getCentreY() ? 0 : 1;
    if (onGesture)
        onGesture(dragging, true);
    dragTap(dragging, e.position.x);
}

void TapDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging >= 0)
        dragTap(dragging, e.position.x);
}

void TapDisplay::mouseUp(const juce::MouseEvent&)
{
    if (dragging >= 0 && onGesture)
        onGesture(dragging, false);
    dragging = -1;
}

void TapDisplay::paint(juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), theme.cornerRadius);
    const float mid = a.getCentreY();
    // tempo grid
    g.setFont(10.0f);
    static const int divisions[] = {1, 2, 4, 8, 16};
    for (int d : divisions)
    {
        const double note = mm::TempoSync::noteMs(tempo, d, false, false);
        if (note > kMaxMs)
            continue;
        const float x = xForMs(note);
        g.setColour(theme.grid);
        g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
        g.setColour(theme.textDim);
        g.drawText("1/" + juce::String(d), juce::Rectangle<float>(x - 20.0f, a.getBottom() - 12.0f, 40.0f, 12.0f), juce::Justification::centred);
    }
    g.setColour(theme.outline);
    g.drawLine(a.getX(), mid, a.getRight(), mid, 1.0f);
    // taps: up to 12 repeats per side, height by fb^n
    const float half = a.getHeight() * 0.5f - 16.0f;
    for (int side = 0; side < 2; ++side)
    {
        const double base = timeMs[side];
        for (int n = 1; n <= 12; ++n)
        {
            double t;
            float amp;
            if (pingPong) // alternating sides at multiples of the left time
            {
                const bool left = n % 2 == 1;
                if ((side == 0) != left)
                    continue;
                t = timeMs[0] * n;
                amp = std::pow(fb, static_cast<float>(n - 1));
            }
            else
            {
                t = base * n;
                amp = std::pow(fb, static_cast<float>(n - 1));
            }
            if (t > kMaxMs || amp < 0.02f)
                break;
            const float x = xForMs(t), h = half * amp;
            g.setColour(n == 1 ? theme.accent : theme.accent.withAlpha(0.6f));
            if (side == 0)
                g.fillRoundedRectangle(x - 3.0f, mid - h, 6.0f, h, 2.0f);
            else
                g.fillRoundedRectangle(x - 3.0f, mid, 6.0f, h, 2.0f);
        }
    }
    g.setColour(theme.text);
    g.setFont(11.0f);
    g.drawText("L " + juce::String(timeMs[0], 1) + " ms", a.withHeight(14.0f), juce::Justification::topLeft);
    g.drawText("R " + juce::String(timeMs[1], 1) + " ms", a.withTrimmedTop(a.getHeight() - 26.0f).withHeight(14.0f), juce::Justification::topLeft);
    if (snapLabel.isNotEmpty())
    {
        g.setColour(theme.accent);
        g.drawText("snapped to " + snapLabel, a.withHeight(14.0f), juce::Justification::topRight);
    }
    if (synced)
    {
        g.setColour(theme.textDim);
        g.drawText("tempo sync on: time follows the host", a.withHeight(14.0f), juce::Justification::centredTop);
    }
}
} // namespace mm::gui
