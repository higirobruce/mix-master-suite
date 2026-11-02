#pragma once
#include "gui/Theme.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::gui
{
// Delay tap timeline (spec: "visual tap display you can drag; tempo-sync grid snaps when dragging near a note
// value"). Left taps above the axis, right taps below, bars fading by the feedback. Drag the first tap of a side to
// set its time; within 5 % of a note value at the current BPM the time snaps and a note label shows.
class TapDisplay : public juce::Component
{
public:
    TapDisplay();
    void setTaps(double msL, double msR, float feedback, bool pingPong, bool synced);
    void setBpm(double bpm) noexcept { tempo = bpm; }
    std::function<void(int channel, double ms)> onTimeChange;
    std::function<void(int channel, bool starting)> onGesture;

    float xForMs(double ms) const noexcept;
    double msForX(float x) const noexcept;
    void dragTap(int channel, float x); // programmatic drag (tests)
    juce::String snappedLabel() const { return snapLabel; }

    void setTheme(const Theme& t);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    static constexpr double kMaxMs = 4000.0;

private:
    juce::Rectangle<float> plotArea() const noexcept { return getLocalBounds().toFloat().reduced(12.0f, 10.0f); }
    double snap(double ms, juce::String& label) const;

    double timeMs[2] = {375.0, 500.0};
    float fb = 0.35f;
    bool pingPong = false, synced = false;
    double tempo = 120.0;
    int dragging = -1;
    juce::String snapLabel;
    Theme theme = Theme::dark(accents::timeSpace);
};
} // namespace mm::gui
