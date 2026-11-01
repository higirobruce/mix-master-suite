#include "gui/HistoryDisplay.h"

namespace mm::gui
{
HistoryDisplay::HistoryDisplay(int capacity) : ring(static_cast<size_t>(juce::jmax(2, capacity))) {}

void HistoryDisplay::push(const MeterFrame& f, bool open)
{
    Point p;
    p.inDb = juce::jmax(f.peakInDb[0], f.peakInDb[1]);
    p.outDb = juce::jmax(f.peakOutDb[0], f.peakOutDb[1]);
    p.grDb = f.gainReductionDb;
    p.open = open;
    ring[static_cast<size_t>(head)] = p;
    head = (head + 1) % static_cast<int>(ring.size());
    count = juce::jmin(count + 1, static_cast<int>(ring.size()));
    repaint();
}

void HistoryDisplay::clear()
{
    head = count = 0;
    repaint();
}

void HistoryDisplay::setThresholdDb(float db)
{
    if (std::abs(db - threshold) < 0.01f)
        return;
    threshold = juce::jlimit(kMinDb, kMaxDb, db);
    repaint();
}

void HistoryDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

float HistoryDisplay::yForDb(float db) const noexcept
{
    const auto a = plotArea();
    return a.getBottom() - a.getHeight() * juce::jlimit(0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
}

float HistoryDisplay::dbForY(float y) const noexcept
{
    const auto a = plotArea();
    return kMinDb + (kMaxDb - kMinDb) * juce::jlimit(0.0f, 1.0f, (a.getBottom() - y) / a.getHeight());
}

void HistoryDisplay::dragThresholdTo(float y)
{
    const float db = juce::jlimit(kMinDb, kMaxDb, dbForY(y));
    threshold = db;
    if (onThresholdChange)
        onThresholdChange(db);
    repaint();
}

void HistoryDisplay::mouseDown(const juce::MouseEvent& e)
{
    dragging = nearThreshold(e.position.y) || true; // anywhere on the plot moves the threshold to the pointer
    if (dragging)
        dragThresholdTo(e.position.y);
}

void HistoryDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging)
        dragThresholdTo(e.position.y);
}

void HistoryDisplay::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(nearThreshold(e.position.y) ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void HistoryDisplay::paint(juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), theme.cornerRadius);
    g.setColour(theme.grid);
    for (float db = kMinDb; db <= kMaxDb + 0.01f; db += 12.0f)
        g.drawHorizontalLine(juce::roundToInt(yForDb(db)), a.getX(), a.getRight());

    const int n = count;
    if (n > 0)
    {
        const float dx = a.getWidth() / static_cast<float>(ring.size() - 1);
        const int cap = static_cast<int>(ring.size());
        auto at = [&](int i) -> const Point& { // i = 0 oldest
            return ring[static_cast<size_t>(((head - n + i) % cap + cap) % cap)];
        };
        const float x0 = a.getRight() - dx * static_cast<float>(n - 1);
        // closed shading
        if (shadeClosed)
        {
            g.setColour(theme.textDim.withAlpha(0.12f));
            for (int i = 0; i < n; ++i)
                if (!at(i).open)
                    g.fillRect(x0 + dx * static_cast<float>(i), a.getY(), juce::jmax(dx, 1.0f), a.getHeight());
        }
        // input level fill
        juce::Path in;
        in.startNewSubPath(x0, a.getBottom());
        for (int i = 0; i < n; ++i)
            in.lineTo(x0 + dx * static_cast<float>(i), yForDb(at(i).inDb));
        in.lineTo(a.getRight(), a.getBottom());
        in.closeSubPath();
        g.setColour(theme.textDim.withAlpha(theme.isDark ? 0.35f : 0.3f));
        g.fillPath(in);
        // output level line
        juce::Path out;
        for (int i = 0; i < n; ++i)
        {
            const juce::Point<float> pt(x0 + dx * static_cast<float>(i), yForDb(at(i).outDb));
            if (i == 0) out.startNewSubPath(pt);
            else out.lineTo(pt);
        }
        g.setColour(theme.text.withAlpha(0.8f));
        g.strokePath(out, juce::PathStrokeType(1.0f));
        // gain reduction from the top
        juce::Path gr;
        gr.startNewSubPath(x0, a.getY());
        for (int i = 0; i < n; ++i)
            gr.lineTo(x0 + dx * static_cast<float>(i), a.getY() + a.getHeight() * juce::jlimit(0.0f, 1.0f, at(i).grDb / kGrRangeDb));
        gr.lineTo(a.getRight(), a.getY());
        gr.closeSubPath();
        g.setColour(theme.accent.withAlpha(0.75f));
        g.fillPath(gr);
    }
    // threshold line
    const float ty = yForDb(threshold);
    g.setColour(theme.accent);
    g.drawLine(a.getX(), ty, a.getRight(), ty, 1.5f);
    g.setFont(10.0f);
    g.drawText(juce::String(threshold, 1) + " dB", juce::Rectangle<float>(a.getX() + 4.0f, ty - 14.0f, 70.0f, 12.0f),
               juce::Justification::centredLeft);
}
} // namespace mm::gui
