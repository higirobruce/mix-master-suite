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

class LpEqProcessor;

// Linear-phase EQ editor: spectrum + draggable curve as the main display with the FIR's resolution floor shaded,
// one column per band in the strip (on, type, frequency / gain / Q, phase, channel), FIR length and the latency
// cost in the advanced drawer, the latency also in the footer (spec: "shows latency cost as you lengthen the FIR").
class LpEqEditor : public mm::gui::PluginEditorBase
{
public:
    explicit LpEqEditor(LpEqProcessor& p);
    juce::String latencyText() const; // e.g. "53.3 ms latency · floor 23 Hz"

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    struct BandColumn
    {
        std::unique_ptr<mm::gui::Toggle> enabled;
        std::unique_ptr<mm::gui::Selector> type, phase, ms;
        std::unique_ptr<mm::gui::Knob> freq, gain, q;
    };
    void updateLatencyLabel();

    LpEqProcessor& eq;
    mm::gui::SpectrumDisplay spectrum;
    mm::gui::CurveDisplay curve;
    std::array<BandColumn, 8> bands;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Selector firLength;
    juce::Label latencyLabel;
};
