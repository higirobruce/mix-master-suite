#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/CeilingHistory.h"
#include "gui/Knob.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"

class LimiterProcessor;

// Limiter editor: the ceiling history (reduction hanging from the ceiling in gold, true-peak overs as red ticks) as
// the main display with the LUFS-I / true-peak / LRA readouts above it; input, ceiling, release, lookahead and the
// auto-release switch with the character tabs in the strip; stereo link and dither in the drawer.
class LimiterEditor : public mm::gui::PluginEditorBase
{
public:
    explicit LimiterEditor(LimiterProcessor& p);
    juce::String readoutText() const;

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    LimiterProcessor& limiter;
    mm::gui::CeilingHistory history;
    juce::Label readouts;
    juce::TextButton resetMeters{"RESET"};
    mm::gui::Knob input, ceiling, release, lookahead;
    mm::gui::Toggle autoRelease;
    mm::gui::Selector character;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Knob link;
    mm::gui::Selector dither;
    juce::Label ditherNote;
};
