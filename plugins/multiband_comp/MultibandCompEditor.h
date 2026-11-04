#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/Knob.h"
#include "gui/MultibandDisplay.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include <array>
#include <memory>

class MultibandCompProcessor;

// Multiband compressor editor: spectrum with draggable crossover lines and per-band reduction shading as the main
// display; click a band to focus its controls in the strip (every band's controls exist, the focused band's are
// shown); bands count, crossovers and mix beside them; detector and external key in the drawer.
class MultibandCompEditor : public mm::gui::PluginEditorBase
{
public:
    explicit MultibandCompEditor(MultibandCompProcessor& p);
    int focusedBand() const noexcept { return focused; }

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    struct BandControls
    {
        std::unique_ptr<mm::gui::Knob> threshold, ratio, attack, release, gain;
        std::unique_ptr<mm::gui::Toggle> solo, bypass;
    };
    void setFocused(int band);
    void layoutBand(BandControls& b, juce::Rectangle<int> area);

    MultibandCompProcessor& comp;
    mm::gui::MultibandDisplay display;
    std::array<BandControls, 5> bands;
    std::array<std::unique_ptr<mm::gui::Knob>, 4> crossovers;
    mm::gui::Selector numBands;
    mm::gui::Knob mix;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Selector detector;
    mm::gui::Toggle scExternal;
    juce::Label bandTitle;
    int focused = 0;
};
