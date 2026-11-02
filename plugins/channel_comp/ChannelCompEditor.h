#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/GainReductionMeter.h"
#include "gui/HistoryDisplay.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"

class ChannelCompProcessor;

// Channel Compressor editor (mockup board 6 without the prompt bar): GR history with a draggable threshold as the
// main display, transfer curve + GR meter beside it, the dynamics strip below, sidechain/routing in the drawer.
class ChannelCompEditor : public mm::gui::PluginEditorBase, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ChannelCompEditor(ChannelCompProcessor& p);
    ~ChannelCompEditor() override;

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    void parameterChanged(const juce::String&, float) override;
    ChannelCompProcessor& comp;
    mm::gui::HistoryDisplay history;
    mm::gui::TransferCurveDisplay curve;
    mm::gui::GainReductionMeter grMeter;
    juce::Label grLabel;
    mm::gui::Knob threshold, ratio, attack, release, knee, mix, makeup;
    mm::gui::Toggle ratioInf, autoRelease, autoMakeup;
    mm::gui::Selector detector;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Selector topology;
    mm::gui::Toggle scHpEnabled, extKey, listenSc;
    mm::gui::Knob scHpHz, stereoLink;
    std::atomic<bool> curveDirty{true};
};
