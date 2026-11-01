#pragma once
#include "gui/Knob.h"
#include "gui/MeterBar.h"
#include "gui/PluginEditorBase.h"
#include "gui/Toggle.h"

class UtilityProcessor;

// Utility: large stereo in/out meters as the main display, gain + phase knobs and routing toggles in the strip.
class UtilityEditor : public mm::gui::PluginEditorBase
{
public:
    explicit UtilityEditor(UtilityProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    class MeterDisplay : public juce::Component
    {
    public:
        MeterDisplay();
        void setTheme(const mm::gui::Theme& t);
        void update(const mm::MeterFrame& f);
        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        mm::gui::Theme theme = mm::gui::Theme::dark(mm::gui::accents::utility);
        mm::gui::MeterBar inL, inR, outL, outR;
        juce::Label inLabel{{}, "INPUT"}, outLabel{{}, "OUTPUT"}, corr;
    };
    MeterDisplay display;
    mm::gui::Knob gain, phase;
    mm::gui::Toggle polarityL, polarityR, swapLR, monoSum, dcBlock, muteL, muteR;
};
