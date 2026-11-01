#pragma once
#include "gui/Theme.h"
#include <complex>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// Log-frequency response curve with draggable band nodes (EQ family): drag = frequency/gain, wheel = Q,
// hold 400 ms = solo. The display owns no DSP: it asks the processor for nodes and the response, and reports edits.
class CurveDisplay : public juce::Component, private juce::Timer
{
public:
    struct Node
    {
        int bandIndex = 0;
        bool enabled = true;
        double freq = 1000.0, gainDb = 0.0, q = 1.0;
        juce::Colour colour;
    };
    using NodeSource = std::function<std::vector<Node>()>;
    using ResponseFn = std::function<std::complex<double>(double hz)>;

    CurveDisplay(NodeSource nodes, ResponseFn response, double minHz = 20.0, double maxHz = 20000.0, double rangeDb = 18.0);

    std::function<void(int bandIndex, double freq, double gainDb)> onNodeMove;
    std::function<void(int bandIndex, double qFactor)> onNodeQ; // multiply the band's Q by qFactor
    std::function<void(int bandIndex, bool solo)> onNodeSolo;

    void setTheme(const Theme& t);
    void refreshFromProcessor(); // re-read nodes and the curve (editor timer / tests)
    int numNodes() const noexcept;                      // enabled bands
    juce::Point<float> nodePosition(int bandIndex) const; // pixel position of a band's node
    void dragNode(int bandIndex, juce::Point<float> to);  // programmatic drag (tests)
    void wheelNode(int bandIndex, float delta);           // programmatic wheel (tests)

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    float xForHz(double hz) const noexcept;
    double hzForX(float x) const noexcept;
    float yForDb(double db) const noexcept;
    double dbForY(float y) const noexcept;

private:
    void timerCallback() override;
    int nodeAt(juce::Point<float> p) const;
    juce::Rectangle<float> plotArea() const noexcept { return getLocalBounds().toFloat().reduced(8.0f, 6.0f); }

    NodeSource nodeSource;
    ResponseFn responseFn;
    std::vector<Node> nodes;
    std::vector<float> curveY; // one y per plot pixel column
    double minHz, maxHz, rangeDb;
    Theme theme = Theme::dark(accents::eq);
    int dragging = -1, soloing = -1;
};
} // namespace mm::gui
