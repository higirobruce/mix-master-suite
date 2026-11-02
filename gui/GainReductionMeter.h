#pragma once
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::gui
{
// Vertical gain-reduction meter: 0 dB at the top, growing downwards to rangeDb, with a slow-falling hold line.
class GainReductionMeter : public juce::Component
{
public:
    explicit GainReductionMeter(float rangeDb = 24.0f) : range(rangeDb) {}
    void setGainReduction(float db);
    float gainReductionDb() const noexcept { return gr; }
    void setTheme(const Theme& t)
    {
        theme = t;
        repaint();
    }
    void paint(juce::Graphics&) override;

private:
    float range, gr = 0.0f, hold = 0.0f;
    Theme theme = Theme::dark(accents::dynamics);
};
} // namespace mm::gui
