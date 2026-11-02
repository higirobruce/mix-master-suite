#pragma once
#include "gui/Theme.h"
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// Harmonic spectrum of the static transfer curve: one bar per harmonic (1..N), even harmonics in the accent colour,
// odd in the text colour, dB scale minDb..0. The editor supplies the levels (computed from parameter values on the
// message thread); the display owns no DSP.
class HarmonicDisplay : public juce::Component
{
public:
    explicit HarmonicDisplay(int numHarmonics = 16, float minDb = -90.0f);
    void setLevels(const float* levelsDb, int count);
    int numBars() const noexcept { return static_cast<int>(levels.size()); }
    float level(int harmonic1Based) const noexcept { return levels[static_cast<size_t>(juce::jlimit(1, numBars(), harmonic1Based) - 1)]; }
    void setTheme(const Theme& t);
    void paint(juce::Graphics&) override;

private:
    std::vector<float> levels;
    float minDb;
    Theme theme = Theme::dark(accents::colour);
};
} // namespace mm::gui
