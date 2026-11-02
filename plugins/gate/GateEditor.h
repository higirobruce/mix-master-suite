#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/GainReductionMeter.h"
#include "gui/HistoryDisplay.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"

class GateProcessor;

// Gate editor (spec UI table: waveform history with open/closed shading, listen-to-what-is-removed): history with
// a draggable threshold as the main display, transfer curve + GR meter beside it, the gate strip below,
// sidechain filters / external key / listen in the drawer.
class GateEditor : public mm::gui::PluginEditorBase, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit GateEditor(GateProcessor& p);
    ~GateEditor() override;

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    void parameterChanged(const juce::String&, float) override;
    GateProcessor& gate;
    mm::gui::HistoryDisplay history;
    mm::gui::TransferCurveDisplay curve;
    mm::gui::GainReductionMeter grMeter;
    juce::Label grLabel, stateLabel;
    mm::gui::Knob threshold, range, attack, hold, release, ratio, hysteresis, lookahead;
    mm::gui::Selector mode;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Toggle scHpEnabled, scLpEnabled, extKey, listenSc;
    mm::gui::Knob scHpHz, scLpHz;
    std::atomic<bool> curveDirty{true};
};
