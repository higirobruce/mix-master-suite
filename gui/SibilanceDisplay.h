#pragma once
#include "core/metering/MeterFrame.h"
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// De-esser history (spec: "waveform history with detected sibilant regions highlighted"): scrolling input peak
// trace, frames flagged as sibilant shaded in the accent, the applied reduction drawn from the top.
class SibilanceDisplay : public juce::Component
{
public:
    explicit SibilanceDisplay(int capacity = 600);
    void push(const MeterFrame& frame, bool sibilant, float reductionDb);
    int numPoints() const noexcept { return count; }
    int sibilantCount() const noexcept;
    void setTheme(const Theme& t);
    void paint(juce::Graphics&) override;
    static constexpr float kMinDb = -60.0f, kMaxDb = 0.0f, kReductionRangeDb = 24.0f;

private:
    struct Point
    {
        float inDb = -120.0f, reductionDb = 0.0f;
        bool sibilant = false;
    };
    juce::Rectangle<float> plotArea() const noexcept { return getLocalBounds().toFloat().reduced(8.0f, 6.0f); }
    float yForDb(float db) const noexcept;
    std::vector<Point> ring;
    int head = 0, count = 0;
    Theme theme = Theme::dark(accents::dynamics);
};
} // namespace mm::gui
