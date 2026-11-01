#pragma once
#include "gui/Knob.h"
#include "gui/MeterBar.h"
#include "gui/Theme.h"
#include "plugin_base/PluginProcessorBase.h"

namespace mm::gui
{
// Shared footer: input/output meters, latency readout, CPU hint, output trim (last in the chain).
class Footer : public juce::Component
{
public:
    explicit Footer(PluginProcessorBase& processor);
    void setTheme(const Theme& t);
    void resized() override;
    void paint(juce::Graphics&) override;
    void update(const MeterFrame& frame);
    void setStatusText(const juce::String& text) { status.setText(text, juce::dontSendNotification); }

private:
    Theme theme = Theme::dark(accents::utility);
    MeterBar inL, inR, outL, outR;
    juce::Label inLabel{{}, "IN"}, outLabel{{}, "OUT"}, inValue, outValue, status;
    Knob trim;
};
} // namespace mm::gui
