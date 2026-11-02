#pragma once
#include "core/metering/MeterFrame.h"
#include "gui/Theme.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// Scrolling dynamics history: input peak level (filled trace, -60..0 dB), gain reduction (from the top, accent
// colour) and a draggable threshold line. Optional open/closed shading (gate). One point per meter frame.
class HistoryDisplay : public juce::Component
{
public:
    explicit HistoryDisplay(int capacity = 600);

    void push(const MeterFrame& frame, bool open = true); // editor timer
    int numPoints() const noexcept { return count; }
    void clear();

    void setThresholdDb(float db); // from the parameter
    float thresholdDb() const noexcept { return threshold; }
    std::function<void(float thresholdDb)> onThresholdChange; // user dragged the line
    std::function<void(bool starting)> onGesture;              // drag start / end (host automation gestures)
    void setShadeClosed(bool shade) noexcept { shadeClosed = shade; }
    bool shadesClosed() const noexcept { return shadeClosed; }

    float yForDb(float db) const noexcept;
    float dbForY(float y) const noexcept;
    void dragThresholdTo(float y); // programmatic drag (tests)

    void setTheme(const Theme& t);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    static constexpr float kMinDb = -60.0f, kMaxDb = 0.0f, kGrRangeDb = 30.0f;

private:
    struct Point
    {
        float inDb = -120.0f, outDb = -120.0f, grDb = 0.0f;
        bool open = true;
    };
    juce::Rectangle<float> plotArea() const noexcept { return getLocalBounds().toFloat().reduced(8.0f, 6.0f); }
    bool nearThreshold(float y) const noexcept { return std::abs(y - yForDb(threshold)) < 6.0f; }

    std::vector<Point> ring;
    int head = 0, count = 0;
    float threshold = -20.0f;
    bool dragging = false, shadeClosed = false;
    Theme theme = Theme::dark(accents::dynamics);
};
} // namespace mm::gui
