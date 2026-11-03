#include "gui/Goniometer.h"
#include <cmath>

namespace mm::gui
{
Goniometer::Goniometer(int capacity) : xs(static_cast<size_t>(juce::jmax(2, capacity))), ys(static_cast<size_t>(juce::jmax(2, capacity))) {}

void Goniometer::pushScope(const float* l, const float* r, int n)
{
    const int cap = static_cast<int>(xs.size());
    for (int i = 0; i < n; ++i)
    {
        xs[static_cast<size_t>(head)] = (l[i] - r[i]) * 0.70710678f;
        ys[static_cast<size_t>(head)] = (l[i] + r[i]) * 0.70710678f;
        head = (head + 1) % cap;
        count = juce::jmin(count + 1, cap);
    }
    repaint();
}

void Goniometer::setCorrelation(float c)
{
    corr = juce::jlimit(-1.0f, 1.0f, c);
    repaint();
}

void Goniometer::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

void Goniometer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, theme.cornerRadius);
    auto bar = bounds.removeFromBottom(static_cast<float>(kBarHeight)).reduced(12.0f, 4.0f);
    const float side = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 16.0f;
    const auto scope = juce::Rectangle<float>(side, side).withCentre(bounds.getCentre());
    const auto c = scope.getCentre();
    const float radius = side * 0.5f;
    // grid: the M axis (vertical), the S axis, the L and R diagonals
    g.setColour(theme.grid);
    g.drawLine(c.x, scope.getY(), c.x, scope.getBottom());
    g.drawLine(scope.getX(), c.y, scope.getRight(), c.y);
    g.drawLine(scope.getX(), scope.getBottom(), scope.getRight(), scope.getY());
    g.drawLine(scope.getX(), scope.getY(), scope.getRight(), scope.getBottom());
    g.drawEllipse(scope, 1.0f);
    g.setColour(theme.textDim);
    g.setFont(10.0f);
    g.drawText("M", juce::Rectangle<float>(c.x - 10.0f, scope.getY() - 2.0f, 20.0f, 12.0f), juce::Justification::centred);
    g.drawText("L", juce::Rectangle<float>(scope.getX() - 2.0f, scope.getY() + 4.0f, 14.0f, 12.0f), juce::Justification::centred);
    g.drawText("R", juce::Rectangle<float>(scope.getRight() - 12.0f, scope.getY() + 4.0f, 14.0f, 12.0f), juce::Justification::centred);
    g.drawText("S", juce::Rectangle<float>(scope.getRight() - 12.0f, c.y - 6.0f, 12.0f, 12.0f), juce::Justification::centred);
    if (count > 0)
    {
        const int cap = static_cast<int>(xs.size());
        const float scale = radius * 0.9f; // full scale = 0 dBFS on the M axis
        for (int i = 0; i < count; ++i)
        {
            const int idx = ((head - count + i) % cap + cap) % cap;
            const float age = static_cast<float>(i + 1) / static_cast<float>(count); // 1 = newest
            const float x = c.x + juce::jlimit(-radius, radius, xs[static_cast<size_t>(idx)] * scale);
            const float y = c.y - juce::jlimit(-radius, radius, ys[static_cast<size_t>(idx)] * scale);
            g.setColour(theme.accent.withAlpha(0.08f + 0.6f * age * age));
            g.fillRect(x - 1.0f, y - 1.0f, 2.0f, 2.0f);
        }
    }
    // correlation bar
    g.setColour(theme.grid);
    g.fillRoundedRectangle(bar, 3.0f);
    const float mid = bar.getCentreX();
    const float px = mid + corr * bar.getWidth() * 0.5f;
    g.setColour(corr < 0.0f ? theme.meterHigh : theme.accent);
    g.fillRoundedRectangle(juce::Rectangle<float>::leftTopRightBottom(juce::jmin(mid, px), bar.getY(), juce::jmax(mid, px), bar.getBottom()), 3.0f);
    g.setColour(theme.text);
    g.fillRect(px - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
    g.setColour(theme.textDim);
    g.setFont(10.0f);
    g.drawText("-1", bar.withWidth(20.0f).translated(-22.0f, 0.0f), juce::Justification::centredRight);
    g.drawText("+1", bar.withX(bar.getRight() + 2.0f).withWidth(20.0f), juce::Justification::centredLeft);
    g.drawText(juce::String(corr, 2), bar.withY(bar.getY() - 14.0f).withHeight(12.0f), juce::Justification::centred);
}
} // namespace mm::gui
