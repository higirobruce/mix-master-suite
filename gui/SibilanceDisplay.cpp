#include "gui/SibilanceDisplay.h"

namespace mm::gui
{
SibilanceDisplay::SibilanceDisplay(int capacity) : ring(static_cast<size_t>(juce::jmax(2, capacity))) {}

void SibilanceDisplay::push(const MeterFrame& f, bool sibilant, float reductionDb)
{
    Point p;
    p.inDb = juce::jmax(f.peakInDb[0], f.peakInDb[1]);
    p.reductionDb = reductionDb;
    p.sibilant = sibilant;
    ring[static_cast<size_t>(head)] = p;
    head = (head + 1) % static_cast<int>(ring.size());
    count = juce::jmin(count + 1, static_cast<int>(ring.size()));
    repaint();
}

int SibilanceDisplay::sibilantCount() const noexcept
{
    int n = 0;
    const int cap = static_cast<int>(ring.size());
    for (int i = 0; i < count; ++i)
        if (ring[static_cast<size_t>(((head - count + i) % cap + cap) % cap)].sibilant)
            ++n;
    return n;
}

void SibilanceDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

float SibilanceDisplay::yForDb(float db) const noexcept
{
    const auto a = plotArea();
    return a.getBottom() - a.getHeight() * juce::jlimit(0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
}

void SibilanceDisplay::paint(juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), theme.cornerRadius);
    g.setColour(theme.grid);
    for (float db = kMinDb; db <= kMaxDb + 0.01f; db += 12.0f)
        g.drawHorizontalLine(juce::roundToInt(yForDb(db)), a.getX(), a.getRight());
    if (count > 0)
    {
        const int cap = static_cast<int>(ring.size());
        const float dx = a.getWidth() / static_cast<float>(cap - 1);
        auto at = [&](int i) -> const Point& { return ring[static_cast<size_t>(((head - count + i) % cap + cap) % cap)]; };
        const float x0 = a.getRight() - dx * static_cast<float>(count - 1);
        // sibilant frames shaded
        g.setColour(theme.accent.withAlpha(0.25f));
        for (int i = 0; i < count; ++i)
            if (at(i).sibilant)
                g.fillRect(x0 + dx * static_cast<float>(i), a.getY(), juce::jmax(dx, 1.0f), a.getHeight());
        // input peak trace
        juce::Path in;
        in.startNewSubPath(x0, a.getBottom());
        for (int i = 0; i < count; ++i)
            in.lineTo(x0 + dx * static_cast<float>(i), yForDb(at(i).inDb));
        in.lineTo(a.getRight(), a.getBottom());
        in.closeSubPath();
        g.setColour(theme.textDim.withAlpha(theme.isDark ? 0.35f : 0.3f));
        g.fillPath(in);
        // reduction from the top
        juce::Path red;
        red.startNewSubPath(x0, a.getY());
        for (int i = 0; i < count; ++i)
            red.lineTo(x0 + dx * static_cast<float>(i), a.getY() + a.getHeight() * juce::jlimit(0.0f, 1.0f, at(i).reductionDb / kReductionRangeDb));
        red.lineTo(a.getRight(), a.getY());
        red.closeSubPath();
        g.setColour(theme.accent.withAlpha(0.8f));
        g.fillPath(red);
    }
    g.setColour(theme.textDim);
    g.setFont(10.0f);
    g.drawText("sibilant frames shaded; reduction from the top", a.withHeight(14.0f), juce::Justification::topRight);
}
} // namespace mm::gui
