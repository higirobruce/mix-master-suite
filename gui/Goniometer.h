#pragma once
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// Imager main display (spec: "vectorscope or goniometer with a correlation bar; width drawn as the spread of the
// Lissajous figure"): the scope ring rotated 45 degrees so mono is vertical and side is horizontal, newest
// samples brightest; a -1..+1 correlation bar underneath.
class Goniometer : public juce::Component
{
public:
    explicit Goniometer(int capacity = 4096);
    void pushScope(const float* l, const float* r, int n); // newest last; keeps the latest `capacity`
    void setCorrelation(float c);
    float correlation() const noexcept { return corr; }
    int numPoints() const noexcept { return count; }
    void setTheme(const Theme& t);
    void paint(juce::Graphics&) override;
    static constexpr int kBarHeight = 22;

private:
    std::vector<float> xs, ys; // rotated: x = side, y = mid
    int head = 0, count = 0;
    float corr = 1.0f;
    Theme theme = Theme::dark(accents::stereo);
};
} // namespace mm::gui
