#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/Goniometer.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/SpectrumDisplay.h"
#include "gui/Toggle.h"
#include <array>
#include <memory>
#include <vector>

class MsProcessor;

// M/S processor editor: goniometer with the correlation bar as the main display and a small output spectrum with
// the mono-below line beside it; mid / side gains, width, mono-below, output and the solo selector in the strip;
// the two EQ bands per channel in the drawer (spec: Imager / M/S row).
class MsEditor : public mm::gui::PluginEditorBase
{
public:
    explicit MsEditor(MsProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    struct BandControls
    {
        std::unique_ptr<mm::gui::Toggle> enabled;
        std::unique_ptr<mm::gui::Selector> type;
        std::unique_ptr<mm::gui::Knob> freq, gain, q;
        juce::Label title;
    };
    MsProcessor& ms;
    mm::gui::Goniometer gonio;
    mm::gui::SpectrumDisplay spectrum;
    mm::gui::Knob mid, side, width, monoBelow, output;
    mm::gui::Selector solo;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    std::array<BandControls, 4> bands; // mid 1, mid 2, side 1, side 2
    std::vector<float> scopeL, scopeR;
};
