#pragma once
#include "gui/AdvancedDrawer.h"
#include "gui/Knob.h"
#include "gui/NeedleMeter.h"
#include "gui/PluginEditorBase.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"

class BusCompProcessor;

// Bus compressor editor: the needle gain-reduction meter as the main display (mockup board 5), threshold / makeup /
// mix knobs with the stepped ratio / attack / release selectors in the strip; sidechain high-pass, oversampling,
// external key and the transfer curve in the drawer.
class BusCompEditor : public mm::gui::PluginEditorBase
{
public:
    explicit BusCompEditor(BusCompProcessor& p);

protected:
    void layoutContent(juce::Rectangle<int> area) override;
    void themeChanged(const mm::gui::Theme&) override;
    void meterFrame(const mm::MeterFrame&) override;

private:
    BusCompProcessor& comp;
    mm::gui::NeedleMeter needle;
    mm::gui::Knob threshold, makeup, mix;
    mm::gui::Selector ratio, attack, release;
    mm::gui::AdvancedDrawer drawer;
    juce::Component drawerContent;
    mm::gui::Knob scHp;
    mm::gui::Selector oversample;
    mm::gui::Toggle scExternal;
    mm::gui::TransferCurveDisplay curve;
};
