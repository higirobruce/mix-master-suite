#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/DecayDisplay.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"

class ReverbProcessor;

// Reverb editor: draggable decay display as the main display; algorithm, pre-delay, decay, size, damping, diffusion,
// early/late, mix in the strip; low/high cut and width in the drawer. Teal accent (time/space family).
class ReverbEditor : public mm::gui::PluginEditorBase
{
public:
    explicit ReverbEditor(ReverbProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    ReverbProcessor& reverb;
    mm::gui::DecayDisplay decayDisplay;
    mm::gui::Selector algorithm;
    mm::gui::Knob predelay, decay, size, damping, diffusion, earlyLate, mix;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Knob lowCut, highCut, width;
};
