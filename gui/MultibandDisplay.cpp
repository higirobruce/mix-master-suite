#include "gui/MultibandDisplay.h"
#include <cmath>

namespace mm::gui
{
MultibandDisplay::MultibandDisplay(SpectrumAnalyser* post, double sampleRate) : spectrum(nullptr, post, sampleRate)
{
    spectrum.setVisiblePrePost(false, true);
    spectrum.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(spectrum);
    reductionDb.fill(0.0f);
    levelDb.fill(-120.0f);
}

void MultibandDisplay::setBands(int numBands, const double* crossoverHz)
{
    bands = juce::jlimit(2, kMaxBands, numBands);
    for (int i = 0; i < bands - 1; ++i)
        crossovers[static_cast<size_t>(i)] = crossoverHz[i];
    if (focused >= bands)
        focused = bands - 1;
    repaint();
}

void MultibandDisplay::setBandReduction(int band, float db, float level)
{
    if (band < 0 || band >= kMaxBands)
        return;
    reductionDb[static_cast<size_t>(band)] = db;
    levelDb[static_cast<size_t>(band)] = level;
    repaint();
}

void MultibandDisplay::focusBand(int band)
{
    focused = juce::jlimit(0, bands - 1, band);
    if (onFocusBand)
        onFocusBand(focused);
    repaint();
}

float MultibandDisplay::xForHz(double hz) const noexcept
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * static_cast<float>(std::log(juce::jlimit(kMinHz, kMaxHz, hz) / kMinHz) / std::log(kMaxHz / kMinHz));
}

double MultibandDisplay::hzForX(float x) const noexcept
{
    const auto a = plotArea();
    const double t = juce::jlimit(0.0, 1.0, static_cast<double>((x - a.getX()) / a.getWidth()));
    return kMinHz * std::pow(kMaxHz / kMinHz, t);
}

void MultibandDisplay::dragCrossoverTo(int crossover, float x)
{
    if (crossover < 0 || crossover >= bands - 1)
        return;
    double hz = hzForX(x);
    const double third = std::pow(2.0, 1.0 / 3.0);
    if (crossover > 0)
        hz = std::max(hz, crossovers[static_cast<size_t>(crossover - 1)] * third);
    if (crossover < bands - 2)
        hz = std::min(hz, crossovers[static_cast<size_t>(crossover + 1)] / third);
    crossovers[static_cast<size_t>(crossover)] = hz;
    if (onCrossoverDrag)
        onCrossoverDrag(crossover, hz);
    repaint();
}

void MultibandDisplay::setTheme(const Theme& t)
{
    theme = t;
    spectrum.setTheme(t);
    repaint();
}

void MultibandDisplay::resized() { spectrum.setBounds(getLocalBounds()); }

void MultibandDisplay::paintOverChildren(juce::Graphics& g)
{
    const auto a = plotArea();
    const juce::Colour bandColours[kMaxBands] = {theme.accent.withAlpha(0.18f), theme.accent.withAlpha(0.14f), theme.accent.withAlpha(0.18f),
                                                 theme.accent.withAlpha(0.14f), theme.accent.withAlpha(0.18f)};
    for (int b = 0; b < bands; ++b)
    {
        const float x0 = b == 0 ? a.getX() : xForHz(crossovers[static_cast<size_t>(b - 1)]);
        const float x1 = b == bands - 1 ? a.getRight() : xForHz(crossovers[static_cast<size_t>(b)]);
        // gain reduction hangs from the top of the plot
        const float h = a.getHeight() * juce::jlimit(0.0f, 1.0f, reductionDb[static_cast<size_t>(b)] / kRangeDb);
        g.setColour(bandColours[b].withMultipliedAlpha(b == focused ? 1.6f : 1.0f));
        g.fillRect(juce::Rectangle<float>(x0, a.getY(), x1 - x0, h));
        if (b == focused)
        {
            g.setColour(theme.accent.withAlpha(0.9f));
            g.drawRect(juce::Rectangle<float>(x0, a.getY(), x1 - x0, a.getHeight()), 1.5f);
        }
        g.setColour(theme.textDim);
        g.setFont(11.0f);
        g.drawText(juce::String(b + 1) + "  " + juce::String(-reductionDb[static_cast<size_t>(b)], 1) + " dB",
                   juce::Rectangle<float>(x0 + 4.0f, a.getBottom() - 16.0f, std::max(10.0f, x1 - x0 - 8.0f), 14.0f), juce::Justification::centredLeft);
    }
    for (int i = 0; i < bands - 1; ++i)
    {
        const float x = xForHz(crossovers[static_cast<size_t>(i)]);
        g.setColour(i == dragging ? theme.accent : theme.accent.withAlpha(0.7f));
        g.drawLine(x, a.getY(), x, a.getBottom(), i == dragging ? 2.0f : 1.2f);
        g.setFont(11.0f);
        g.setColour(theme.text);
        g.drawText(juce::String(juce::roundToInt(crossovers[static_cast<size_t>(i)])) + " Hz",
                   juce::Rectangle<float>(x + 4.0f, a.getY() + 2.0f, 70.0f, 14.0f), juce::Justification::centredLeft);
    }
}

int MultibandDisplay::crossoverAt(juce::Point<float> p) const
{
    for (int i = 0; i < bands - 1; ++i)
        if (std::abs(p.x - xForHz(crossovers[static_cast<size_t>(i)])) < 6.0f)
            return i;
    return -1;
}

int MultibandDisplay::bandAt(float x) const
{
    for (int i = 0; i < bands - 1; ++i)
        if (x < xForHz(crossovers[static_cast<size_t>(i)]))
            return i;
    return bands - 1;
}

void MultibandDisplay::mouseDown(const juce::MouseEvent& e)
{
    dragging = crossoverAt(e.position);
    if (dragging >= 0)
    {
        if (onCrossoverGesture)
            onCrossoverGesture(dragging, true);
        return;
    }
    focusBand(bandAt(e.position.x));
}

void MultibandDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging >= 0)
        dragCrossoverTo(dragging, e.position.x);
}

void MultibandDisplay::mouseUp(const juce::MouseEvent&)
{
    if (dragging >= 0 && onCrossoverGesture)
        onCrossoverGesture(dragging, false);
    dragging = -1;
    repaint();
}
} // namespace mm::gui
