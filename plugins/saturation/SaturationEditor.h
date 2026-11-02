#pragma once
#include "gui/HarmonicDisplay.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"

class SaturationProcessor;

// Saturation editor (spec UI row): one large drive knob, a harmonic spectrum (even/odd coloured) as the main display,
// the character as a selector with a one-line description, bias / HF roll-off / mix / output in the strip.
class SaturationEditor : public mm::gui::PluginEditorBase, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SaturationEditor(SaturationProcessor& p);
    ~SaturationEditor() override;
    juce::String characterDescription() const { return description.getText(); }
    void refreshForTests() { refreshHarmonics(); }

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    void parameterChanged(const juce::String&, float) override;
    void refreshHarmonics();
    SaturationProcessor& sat;
    mm::gui::Knob drive, bias, hfRolloff, mix, output;
    mm::gui::Selector character;
    juce::Label description, title;
    mm::gui::HarmonicDisplay harmonics;
    std::atomic<bool> dirty{true};
};
