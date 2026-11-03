#pragma once
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::gui
{
// VU-style gain-reduction needle (mockup board 5): a cream dial, the needle at rest on the right (0 dB) swinging
// left with reduction, 300 ms ballistics (99 % of a step), driven by the editor timer through advance().
class NeedleMeter : public juce::Component
{
public:
    explicit NeedleMeter(float rangeDb = 20.0f) : range(rangeDb) {}
    void setTarget(float reductionDb) noexcept { target = -std::abs(reductionDb); } // reduction shown as a negative dB
    void advance(double seconds) noexcept;                                        // ballistics: 99 % in 0.3 s
    float value() const noexcept { return shown; }
    void setTheme(const Theme& t)
    {
        theme = t;
        repaint();
    }
    void paint(juce::Graphics&) override;

private:
    Theme theme = Theme::dark(accents::mastering);
    float range, target = 0.0f, shown = 0.0f;
    static constexpr double kTauSeconds = 0.3 / 4.605170185988092; // ln(100): 99 % in 300 ms
};
} // namespace mm::gui
