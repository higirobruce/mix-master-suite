#pragma once
#include "gui/Theme.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::gui
{
// Static input->output transfer curve of a dynamics processor (dB in on x, dB out on y, -60..0) plus a live dot at
// the current input level. The display owns no DSP: it asks the processor for the static gain at a level.
class TransferCurveDisplay : public juce::Component
{
public:
    using GainFn = std::function<float(float levelDb)>; // static gain (dB, <= 0 for a compressor) at an input level

    explicit TransferCurveDisplay(GainFn gain, float minDb = -60.0f, float maxDb = 0.0f);

    float outputDbFor(float inputDb) const { return inputDb + gainFn(inputDb); }
    void setLive(float inputDb, float gainReductionDb); // editor timer
    void refresh() { repaint(); }                     // parameters changed
    void setTheme(const Theme& t);
    void paint(juce::Graphics&) override;

    float xForDb(float db) const noexcept;
    float yForDb(float db) const noexcept;

private:
    juce::Rectangle<float> plotArea() const noexcept { return getLocalBounds().toFloat().reduced(8.0f, 6.0f); }
    GainFn gainFn;
    float minDb, maxDb;
    float liveIn = -120.0f, liveGr = 0.0f;
    Theme theme = Theme::dark(accents::dynamics);
};
} // namespace mm::gui
