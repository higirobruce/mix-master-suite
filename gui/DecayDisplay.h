#pragma once
#include "gui/Theme.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// Reverb decay display (spec: "decay display you can drag"): the analytic envelope exp(-6.91 t / RT60) in dB with
// the early/late split shaded, plus a live wet RMS history. Drag the -60 dB point horizontally to set the decay.
class DecayDisplay : public juce::Component
{
public:
    DecayDisplay();
    void setDecay(double seconds, double predelayMs, float earlyShare);
    void pushLevel(float wetDb); // live tail history (editor timer)
    std::function<void(double seconds)> onDecayChange;
    std::function<void(bool starting)> onGesture;

    float xForSeconds(double s) const noexcept;
    double secondsForX(float x) const noexcept;
    float envelopeDb(double t) const noexcept; // analytic late envelope, 0 dB at t = 0, -60 dB at t = decay
    void dragDecayTo(float x);

    void setTheme(const Theme& t);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const noexcept { return getLocalBounds().toFloat().reduced(12.0f, 10.0f); }
    // the axis spans twice the decay (frozen while dragging so the point stays under the pointer)
    double axisSeconds() const noexcept { return dragging ? dragAxis : juce::jmax(1.0, decay * 2.0); }
    double dragAxis = 4.0;
    double decay = 2.0, predelayMs = 20.0;
    float early = 0.4f;
    std::vector<float> history;
    int head = 0, count = 0;
    bool dragging = false;
    Theme theme = Theme::dark(accents::timeSpace);
};
} // namespace mm::gui
