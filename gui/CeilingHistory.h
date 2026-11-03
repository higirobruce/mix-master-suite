#pragma once
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// Limiter history (mockup board 2): the gain reduction hangs from the ceiling line at the top in gold, scrolling
// left; true-peak overs are red ticks on the timeline. push() once per meter frame.
class CeilingHistory : public juce::Component
{
public:
    explicit CeilingHistory(int capacity = 600, float rangeDb = 12.0f);
    void push(float reductionDb, bool over);
    void clear();
    int numPoints() const noexcept { return count; }
    float latestReductionDb() const noexcept;
    int overCount() const noexcept { return overs; }
    void setTheme(const Theme& t)
    {
        theme = t;
        repaint();
    }
    void paint(juce::Graphics&) override;

private:
    Theme theme = Theme::dark(accents::mastering);
    std::vector<float> reduction;
    std::vector<unsigned char> overFlag;
    int head = 0, count = 0, overs = 0;
    float range;
};
} // namespace mm::gui
