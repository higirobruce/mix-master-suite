#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/GainReductionMeter.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"

class SoftClipperProcessor;

// Soft clipper editor: the clip curve with the live input dot as the main display and a clip-amount meter beside
// it; input / ceiling / knee / mix knobs with the delta button prominent in the strip; oversampling and the true-peak
// ceiling in the drawer.
class SoftClipperEditor : public mm::gui::PluginEditorBase
{
public:
    explicit SoftClipperEditor(SoftClipperProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    SoftClipperProcessor& clipper;
    mm::gui::TransferCurveDisplay curve;
    mm::gui::GainReductionMeter clipMeter;
    juce::Label clipLabel;
    mm::gui::Knob input, ceiling, knee, mix;
    mm::gui::Toggle delta;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Selector oversample;
    mm::gui::Toggle truePeak;
};
