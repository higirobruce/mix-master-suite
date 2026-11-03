#include "plugins/bus_comp/BusCompEditor.h"
#include "Params.h"
#include "plugins/bus_comp/BusCompProcessor.h"

namespace bp = mm::plugins::bus_comp;

BusCompEditor::BusCompEditor(BusCompProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::mastering), 920, 580), comp(p),
      threshold(p.apvts(), bp::id::threshold_db, "THRESHOLD"), makeup(p.apvts(), bp::id::makeup_db, "MAKEUP"), mix(p.apvts(), bp::id::mix_pct, "MIX"),
      ratio(p.apvts(), bp::id::ratio, "RATIO"), attack(p.apvts(), bp::id::attack, "ATTACK"), release(p.apvts(), bp::id::release, "RELEASE"),
      drawer("Advanced"), scHp(p.apvts(), bp::id::sc_hp_hz, "SC HP"), oversample(p.apvts(), bp::id::oversample, "OVERSAMPLING"),
      scExternal(p.apvts(), bp::id::sc_external, "EXT KEY"), curve([this](float levelDb) { return comp.staticGainDb(levelDb); })
{
    auto& c = content();
    c.addAndMakeVisible(needle);
    for (auto* comp_ : {static_cast<juce::Component*>(&threshold), static_cast<juce::Component*>(&makeup), static_cast<juce::Component*>(&mix),
                        static_cast<juce::Component*>(&ratio), static_cast<juce::Component*>(&attack), static_cast<juce::Component*>(&release)})
        c.addAndMakeVisible(comp_);
    drawerContent.addAndMakeVisible(scHp);
    drawerContent.addAndMakeVisible(oversample);
    drawerContent.addAndMakeVisible(scExternal);
    drawerContent.addAndMakeVisible(curve);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(130);
    drawer.setOpen(static_cast<bool>(comp.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { comp.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    needle.setTheme(theme());
    drawer.setTheme(theme());
    curve.setTheme(theme());
    initialise();
}

void BusCompEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter, 4);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    drawerContent.setBounds(drawerArea.withZeroOrigin().withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    auto row = drawerContent.getLocalBounds().withHeight(130).reduced(8, 4);
    curve.setBounds(row.removeFromRight(220).reduced(4));
    scHp.setBounds(row.removeFromLeft(110).reduced(4, 4));
    oversample.setBounds(row.removeFromLeft(200).withTrimmedTop(40).withHeight(40).reduced(4, 0));
    scExternal.setBounds(row.removeFromLeft(140).withTrimmedTop(40).withHeight(40).reduced(4, 0));
    area.removeFromBottom(4);
    auto strip = area.removeFromBottom(124);
    const int colW = strip.getWidth() / 6;
    threshold.setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    ratio.setBounds(strip.removeFromLeft(colW).withTrimmedTop(30).withHeight(40).reduced(4, 0));
    attack.setBounds(strip.removeFromLeft(colW).withTrimmedTop(30).withHeight(40).reduced(4, 0));
    release.setBounds(strip.removeFromLeft(colW).withTrimmedTop(30).withHeight(40).reduced(4, 0));
    makeup.setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    mix.setBounds(strip.reduced(4, 0));
    area.removeFromBottom(6);
    needle.setBounds(area.reduced(area.getWidth() / 5, 0));
}

void BusCompEditor::themeChanged(const mm::gui::Theme& t)
{
    needle.setTheme(t);
    drawer.setTheme(t);
    curve.setTheme(t);
}

void BusCompEditor::meterFrame(const mm::MeterFrame&)
{
    needle.setTarget(comp.currentGainReductionDb());
    needle.advance(1.0 / 30.0);
    curve.refresh();
}
