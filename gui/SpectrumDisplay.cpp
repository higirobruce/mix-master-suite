#include "gui/SpectrumDisplay.h"
#include <cmath>

namespace mm::gui
{
SpectrumDisplay::SpectrumDisplay(mm::SpectrumAnalyser* pre, mm::SpectrumAnalyser* post, double sr, double lo, double hi)
    : preAnalyser(pre), postAnalyser(post), sampleRate(sr), minHz(lo), maxHz(hi)
{
    setOpaque(false);
    setInterceptsMouseClicks(false, false);
}

void SpectrumDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

void SpectrumDisplay::setVisiblePrePost(bool pre, bool post) noexcept
{
    showPreFlag = pre;
    showPostFlag = post;
}

void SpectrumDisplay::pull()
{
    bool changed = false;
    if (preAnalyser != nullptr)
        while (preAnalyser->nextFrame(preMag))
            changed = true;
    if (postAnalyser != nullptr)
        while (postAnalyser->nextFrame(postMag))
            changed = true;
    if (changed)
        repaint();
}

void SpectrumDisplay::drawSpectrum(juce::Graphics& g, const std::vector<float>& magDb, juce::Colour colour, bool fill)
{
    if (magDb.size() < 2)
        return;
    const auto a = getLocalBounds().toFloat().reduced(8.0f, 6.0f);
    const double binHz = sampleRate / (2.0 * static_cast<double>(magDb.size() - 1));
    juce::Path p;
    bool started = false;
    const int w = static_cast<int>(a.getWidth());
    for (int i = 0; i < w; ++i)
    {
        const double hz = minHz * std::pow(maxHz / minHz, static_cast<double>(i) / w);
        const double bin = hz / binHz;
        const size_t k = static_cast<size_t>(std::clamp(bin, 0.0, static_cast<double>(magDb.size() - 1)));
        const float db = std::clamp(magDb[k], -90.0f, 6.0f);
        const float y = a.getBottom() - (db + 90.0f) / 96.0f * a.getHeight();
        const float x = a.getX() + static_cast<float>(i);
        if (!started)
        {
            p.startNewSubPath(x, y);
            started = true;
        }
        else
            p.lineTo(x, y);
    }
    if (fill)
    {
        juce::Path f = p;
        f.lineTo(a.getRight(), a.getBottom());
        f.lineTo(a.getX(), a.getBottom());
        f.closeSubPath();
        g.setColour(colour.withAlpha(0.18f));
        g.fillPath(f);
    }
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(1.0f));
}

void SpectrumDisplay::paint(juce::Graphics& g)
{
    if (showPreFlag)
        drawSpectrum(g, preMag, theme.textDim.withAlpha(0.5f), false);
    if (showPostFlag)
        drawSpectrum(g, postMag, theme.accent.withAlpha(0.7f), true);
    if (marker > 0.0f)
    {
        const auto a = getLocalBounds().toFloat().reduced(8.0f, 6.0f);
        const double x = a.getX() + a.getWidth() * std::log(juce::jlimit(minHz, maxHz, static_cast<double>(marker)) / minHz) / std::log(maxHz / minHz);
        g.setColour(theme.accent);
        g.drawLine(static_cast<float>(x), a.getY(), static_cast<float>(x), a.getBottom(), 1.5f);
        g.setFont(10.0f);
        g.drawText(markerPrefix + juce::String(juce::roundToInt(marker)) + " Hz", juce::Rectangle<float>(static_cast<float>(x) + 4.0f, a.getY(), 90.0f, 14.0f), juce::Justification::centredLeft);
    }
}

void SpectrumDisplay::setMarkerHz(float hz)
{
    marker = hz;
    repaint();
}
} // namespace mm::gui
