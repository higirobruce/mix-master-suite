#pragma once
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::gui
{
// Vertical level bar: RMS as a solid fill, peak as a line, clip indicator above 0 dBFS. Range -60..+6 dB.
class MeterBar : public juce::Component
{
public:
    void setLevels(float peakDb, float rmsDb);
    void setTheme(const Theme& t) { theme = t; repaint(); }
    void paint(juce::Graphics&) override;
    static float dbToY(float db, float height) noexcept;

private:
    Theme theme = Theme::dark(accents::utility);
    float peak = -120.0f, rms = -120.0f, peakHold = -120.0f;
    bool clipped = false;
};
} // namespace mm::gui
