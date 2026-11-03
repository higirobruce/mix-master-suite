#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/Goniometer.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/SpectrumDisplay.h"

class ImagerProcessor;

// Imager editor: goniometer with correlation bar as the main display, a small output spectrum carrying the
// mono-below line; strip: width low / crossover / width mid / crossover / width high / pan / output; drawer:
// mono-below.
class ImagerEditor : public mm::gui::PluginEditorBase
{
public:
    explicit ImagerEditor(ImagerProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    ImagerProcessor& imager;
    mm::gui::Goniometer gonio;
    mm::gui::SpectrumDisplay spectrum;
    mm::gui::Knob widthLow, xoverLow, widthMid, xoverHigh, widthHigh, pan, output, monoBelow;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    std::vector<float> scopeL, scopeR;
};
