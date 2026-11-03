#include "gui/CeilingHistory.h"
#include <algorithm>

namespace mm::gui
{
CeilingHistory::CeilingHistory(int capacity, float rangeDb)
    : reduction(static_cast<size_t>(std::max(2, capacity)), 0.0f), overFlag(static_cast<size_t>(std::max(2, capacity)), 0), range(rangeDb)
{
}

void CeilingHistory::push(float reductionDb, bool over)
{
    const int cap = static_cast<int>(reduction.size());
    if (count == cap && overFlag[static_cast<size_t>(head)])
        --overs; // the oldest point scrolls off
    reduction[static_cast<size_t>(head)] = reductionDb;
    overFlag[static_cast<size_t>(head)] = over ? 1 : 0;
    if (over)
        ++overs;
    head = (head + 1) % cap;
    count = std::min(count + 1, cap);
    repaint();
}

void CeilingHistory::clear()
{
    std::fill(reduction.begin(), reduction.end(), 0.0f);
    std::fill(overFlag.begin(), overFlag.end(), 0);
    head = count = overs = 0;
    repaint();
}

float CeilingHistory::latestReductionDb() const noexcept
{
    if (count == 0)
        return 0.0f;
    const int cap = static_cast<int>(reduction.size());
    return reduction[static_cast<size_t>((head - 1 + cap) % cap)];
}

void CeilingHistory::paint(juce::Graphics& g)
{
    const auto a = getLocalBounds().toFloat().reduced(8.0f, 6.0f);
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    const juce::Colour gold(0xffd4a94a), red(0xffd9534f);
    // grid: 3 dB steps hanging from the ceiling
    g.setColour(theme.textDim.withAlpha(0.35f));
    g.setFont(11.0f);
    for (float db = 0.0f; db <= range; db += 3.0f)
    {
        const float y = a.getY() + a.getHeight() * (db / range);
        g.drawHorizontalLine(static_cast<int>(y), a.getX(), a.getRight());
        g.drawText("-" + juce::String(static_cast<int>(db)) + " dB", juce::Rectangle<float>(a.getRight() - 48.0f, y + 1.0f, 46.0f, 12.0f), juce::Justification::centredRight);
    }
    // the ceiling line
    g.setColour(gold);
    g.drawLine(a.getX(), a.getY(), a.getRight(), a.getY(), 1.5f);
    if (count == 0)
        return;
    const int cap = static_cast<int>(reduction.size());
    const float dx = a.getWidth() / static_cast<float>(cap);
    juce::Path fill;
    fill.startNewSubPath(a.getRight() - dx * static_cast<float>(count), a.getY());
    for (int i = 0; i < count; ++i)
    {
        const int idx = (head - count + i + cap) % cap;
        const float x = a.getRight() - dx * static_cast<float>(count - i);
        const float y = a.getY() + a.getHeight() * juce::jlimit(0.0f, 1.0f, reduction[static_cast<size_t>(idx)] / range);
        fill.lineTo(x, y);
    }
    fill.lineTo(a.getRight(), a.getY());
    fill.closeSubPath();
    g.setColour(gold.withAlpha(0.35f));
    g.fillPath(fill);
    g.setColour(gold);
    g.strokePath(fill, juce::PathStrokeType(1.2f));
    for (int i = 0; i < count; ++i)
    {
        const int idx = (head - count + i + cap) % cap;
        if (overFlag[static_cast<size_t>(idx)])
        {
            const float x = a.getRight() - dx * static_cast<float>(count - i);
            g.setColour(red);
            g.drawLine(x, a.getY() - 5.0f, x, a.getY() + 4.0f, 2.0f);
        }
    }
}
} // namespace mm::gui
