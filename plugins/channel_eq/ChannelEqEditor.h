#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/CurveDisplay.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/SpectrumDisplay.h"
#include "gui/Toggle.h"
#include <array>
#include <memory>

class ChannelEqProcessor;

// Channel EQ editor: spectrum + draggable curve as the main display, one column per band/filter in the strip,
// Q mode / oversampling / analyser in the advanced drawer (mockup board 1).
class ChannelEqEditor : public mm::gui::PluginEditorBase
{
public:
    explicit ChannelEqEditor(ChannelEqProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    struct BandColumn
    {
        std::unique_ptr<mm::gui::Toggle> enabled;
        std::unique_ptr<mm::gui::Selector> type;
        std::unique_ptr<mm::gui::Knob> freq, gain, q;
        juce::Label title;
    };
    struct FilterColumn
    {
        std::unique_ptr<mm::gui::Toggle> enabled;
        std::unique_ptr<mm::gui::Knob> freq;
        std::unique_ptr<mm::gui::Selector> slope;
        juce::Label title;
    };
    ChannelEqProcessor& eq;
    mm::gui::SpectrumDisplay spectrum;
    mm::gui::CurveDisplay curve;
    std::array<BandColumn, 5> bands;
    FilterColumn hp, lp;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Selector qMode, oversample, analyser;
    int soloBand = -1;
    std::array<bool, 5> soloSaved{};
};
