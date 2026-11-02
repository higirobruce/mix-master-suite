#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/TapDisplay.h"
#include "gui/Toggle.h"

class DelayProcessor;

// Delay editor: draggable tap timeline with the tempo grid as the main display; time L/R, link, sync, feedback,
// ping-pong, mix in the strip; feedback filters, modulation and ducking in the drawer.
class DelayEditor : public mm::gui::PluginEditorBase
{
public:
    explicit DelayEditor(DelayProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    DelayProcessor& delay;
    mm::gui::TapDisplay taps;
    mm::gui::Knob timeL, timeR, feedback, mix;
    mm::gui::Toggle link, pingPong;
    mm::gui::Selector sync;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Knob fbLp, fbHp, modDepth, modRate, duckAmount, duckRelease;
};
