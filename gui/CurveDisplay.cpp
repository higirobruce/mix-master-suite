#include "gui/CurveDisplay.h"
#include <cmath>

namespace mm::gui
{
CurveDisplay::CurveDisplay(NodeSource n, ResponseFn r, double lo, double hi, double range)
    : nodeSource(std::move(n)), responseFn(std::move(r)), minHz(lo), maxHz(hi), rangeDb(range)
{
    setOpaque(false);
    refreshFromProcessor();
}

void CurveDisplay::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

float CurveDisplay::xForHz(double hz) const noexcept
{
    const auto a = plotArea();
    const double t = std::log(std::clamp(hz, minHz, maxHz) / minHz) / std::log(maxHz / minHz);
    return a.getX() + static_cast<float>(t) * a.getWidth();
}
double CurveDisplay::hzForX(float x) const noexcept
{
    const auto a = plotArea();
    const double t = std::clamp((x - a.getX()) / a.getWidth(), 0.0f, 1.0f);
    return minHz * std::pow(maxHz / minHz, t);
}
float CurveDisplay::yForDb(double db) const noexcept
{
    const auto a = plotArea();
    return a.getCentreY() - static_cast<float>(std::clamp(db, -rangeDb, rangeDb) / rangeDb) * a.getHeight() * 0.5f;
}
double CurveDisplay::dbForY(float y) const noexcept
{
    const auto a = plotArea();
    return std::clamp(static_cast<double>((a.getCentreY() - y) / (a.getHeight() * 0.5f)) * rangeDb, -rangeDb, rangeDb);
}

void CurveDisplay::refreshFromProcessor()
{
    if (nodeSource)
        nodes = nodeSource();
    const int w = juce::jmax(1, static_cast<int>(plotArea().getWidth()));
    curveY.resize(static_cast<size_t>(w));
    if (responseFn)
        for (int i = 0; i < w; ++i)
        {
            const double hz = hzForX(plotArea().getX() + static_cast<float>(i));
            const double db = 20.0 * std::log10(std::abs(responseFn(hz)) + 1e-12);
            curveY[static_cast<size_t>(i)] = yForDb(db);
        }
    repaint();
}

int CurveDisplay::numNodes() const noexcept
{
    int n = 0;
    for (const auto& node : nodes)
        n += node.enabled ? 1 : 0;
    return n;
}

juce::Point<float> CurveDisplay::nodePosition(int bandIndex) const
{
    for (const auto& node : nodes)
        if (node.bandIndex == bandIndex)
            return {xForHz(node.freq), yForDb(node.gainDb)};
    return {};
}

void CurveDisplay::dragNode(int bandIndex, juce::Point<float> to)
{
    if (onNodeMove)
        onNodeMove(bandIndex, hzForX(to.x), dbForY(to.y));
    refreshFromProcessor();
}

void CurveDisplay::wheelNode(int bandIndex, float delta)
{
    if (onNodeQ)
        onNodeQ(bandIndex, std::pow(1.15, static_cast<double>(delta)));
    refreshFromProcessor();
}

int CurveDisplay::nodeAt(juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 12.0f;
    for (const auto& node : nodes)
    {
        if (!node.enabled)
            continue;
        const float d = nodePosition(node.bandIndex).getDistanceFrom(p);
        if (d < bestDist)
        {
            bestDist = d;
            best = node.bandIndex;
        }
    }
    return best;
}

void CurveDisplay::mouseDown(const juce::MouseEvent& e)
{
    dragging = nodeAt(e.position);
    if (dragging >= 0)
    {
        if (onGesture)
            onGesture(dragging, true);
        startTimer(400); // hold = solo
    }
}
void CurveDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging >= 0)
    {
        stopTimer();
        dragNode(dragging, e.position);
    }
}
void CurveDisplay::mouseUp(const juce::MouseEvent&)
{
    stopTimer();
    if (soloing >= 0 && onNodeSolo)
        onNodeSolo(soloing, false);
    soloing = -1;
    if (dragging >= 0 && onGesture)
        onGesture(dragging, false);
    dragging = -1;
}
void CurveDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int n = nodeAt(e.position);
    if (n >= 0)
        wheelNode(n, w.deltaY * 10.0f);
}
void CurveDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int n = nodeAt(e.position);
    if (n >= 0 && onNodeMove)
        for (const auto& node : nodes)
            if (node.bandIndex == n)
                onNodeMove(n, node.freq, 0.0); // double-click resets the gain
    refreshFromProcessor();
}
void CurveDisplay::timerCallback()
{
    stopTimer();
    if (dragging >= 0 && onNodeSolo)
    {
        soloing = dragging;
        onNodeSolo(soloing, true);
    }
}

void CurveDisplay::resized() { refreshFromProcessor(); }

void CurveDisplay::paint(juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour(theme.grid);
    for (double hz : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0})
        g.drawVerticalLine(static_cast<int>(xForHz(hz)), a.getY(), a.getBottom());
    for (double db : {-12.0, -6.0, 0.0, 6.0, 12.0})
        g.drawHorizontalLine(static_cast<int>(yForDb(db)), a.getX(), a.getRight());
    g.setColour(theme.textDim);
    g.setFont(juce::FontOptions(10.0f));
    for (auto [hz, label] : {std::pair{50.0, "50"}, {100.0, "100"}, {200.0, "200"}, {500.0, "500"}, {1000.0, "1k"},
                             {2000.0, "2k"}, {5000.0, "5k"}, {10000.0, "10k"}})
        g.drawText(label, static_cast<int>(xForHz(hz)) - 14, static_cast<int>(a.getBottom()) - 12, 28, 12, juce::Justification::centred);
    for (double db : {-12.0, -6.0, 0.0, 6.0, 12.0})
        g.drawText(juce::String(static_cast<int>(db)), static_cast<int>(a.getX()), static_cast<int>(yForDb(db)) - 6, 24, 12,
                   juce::Justification::centredLeft);

    if (!curveY.empty())
    {
        juce::Path fill, line;
        line.startNewSubPath(a.getX(), curveY[0]);
        fill.startNewSubPath(a.getX(), yForDb(0.0));
        fill.lineTo(a.getX(), curveY[0]);
        for (size_t i = 1; i < curveY.size(); ++i)
        {
            line.lineTo(a.getX() + static_cast<float>(i), curveY[i]);
            fill.lineTo(a.getX() + static_cast<float>(i), curveY[i]);
        }
        fill.lineTo(a.getX() + static_cast<float>(curveY.size() - 1), yForDb(0.0));
        fill.closeSubPath();
        g.setColour(theme.accent.withAlpha(0.12f));
        g.fillPath(fill);
        g.setColour(theme.accent);
        g.strokePath(line, juce::PathStrokeType(2.0f));
    }
    for (const auto& node : nodes)
    {
        const auto p = nodePosition(node.bandIndex);
        const float r = node.bandIndex == dragging ? 8.0f : 6.0f;
        g.setColour(node.enabled ? node.colour : theme.textDim.withAlpha(0.4f));
        g.fillEllipse(p.x - r, p.y - r, 2 * r, 2 * r);
        g.setColour(theme.bg);
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(juce::String(node.bandIndex + 1), static_cast<int>(p.x) - 6, static_cast<int>(p.y) - 6, 12, 12,
                   juce::Justification::centred);
    }
}
} // namespace mm::gui
