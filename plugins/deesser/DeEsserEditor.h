#pragma once
#include "gui/GainReductionMeter.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/SibilanceDisplay.h"
#include "gui/Toggle.h"

class DeEsserProcessor;

// De-esser editor: sibilance history as the main display, listen prominent, GR meter; strip: frequency, threshold,
// range, width, mode, lookahead.
class DeEsserEditor : public mm::gui::PluginEditorBase
{
public:
    explicit DeEsserEditor(DeEsserProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    DeEsserProcessor& deesser;
    mm::gui::SibilanceDisplay history;
    mm::gui::GainReductionMeter grMeter;
    juce::Label grLabel;
    mm::gui::Knob freq, threshold, range, width, lookahead;
    mm::gui::Selector mode;
    mm::gui::Toggle listen;
};
