#include "plugins/soft_clipper/SoftClipperEditor.h"
#include "Params.h"
#include "core/metering/MeterFrame.h"
#include "plugins/soft_clipper/SoftClipperProcessor.h"

namespace sc = mm::plugins::soft_clipper;

SoftClipperEditor::SoftClipperEditor(SoftClipperProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::mastering), 920, 580), clipper(p),
      curve([this](float levelDb) { return clipper.staticGainDb(levelDb); }, -36.0f, 12.0f), clipMeter(12.0f),
      input(p.apvts(), sc::id::input_db, "INPUT"), ceiling(p.apvts(), sc::id::ceiling_db, "CEILING"), knee(p.apvts(), sc::id::knee_pct, "KNEE"),
      mix(p.apvts(), sc::id::mix_pct, "MIX"), delta(p.apvts(), sc::id::listen_delta, "DELTA"), drawer("Advanced"),
      oversample(p.apvts(), sc::id::oversample, "OVERSAMPLING"), truePeak(p.apvts(), sc::id::true_peak, "TRUE-PEAK CEILING")
{
    auto& c = content();
    c.addAndMakeVisible(curve);
    c.addAndMakeVisible(clipMeter);
    c.addAndMakeVisible(clipLabel);
    clipLabel.setText("CLIP", juce::dontSendNotification);
    clipLabel.setJustificationType(juce::Justification::centred);
    for (auto* comp_ : {static_cast<juce::Component*>(&input), static_cast<juce::Component*>(&ceiling), static_cast<juce::Component*>(&knee),
                        static_cast<juce::Component*>(&mix), static_cast<juce::Component*>(&delta)})
        c.addAndMakeVisible(comp_);
    drawerContent.addAndMakeVisible(oversample);
    drawerContent.addAndMakeVisible(truePeak);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(44);
    drawer.setOpen(static_cast<bool>(clipper.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { clipper.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    curve.setTheme(theme());
    clipMeter.setTheme(theme());
    drawer.setTheme(theme());
    initialise();
}

void SoftClipperEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter, 4);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    drawerContent.setBounds(drawerArea.withZeroOrigin().withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    auto row = drawerContent.getLocalBounds().withHeight(44).reduced(8, 4);
    oversample.setBounds(row.removeFromLeft(220).reduced(4, 0));
    truePeak.setBounds(row.removeFromLeft(220).reduced(4, 4));
    area.removeFromBottom(4);
    auto strip = area.removeFromBottom(120);
    const int kw = strip.getWidth() / 5;
    for (auto* k : {&input, &ceiling, &knee, &mix})
        k->setBounds(strip.removeFromLeft(kw).reduced(4, 0));
    delta.setBounds(strip.reduced(4, 0).withTrimmedTop(36).withHeight(44));
    area.removeFromBottom(6);
    auto meterCol = area.removeFromRight(44);
    clipLabel.setBounds(meterCol.removeFromTop(16));
    clipMeter.setBounds(meterCol.reduced(4, 0));
    area.removeFromRight(6);
    curve.setBounds(area);
}

void SoftClipperEditor::themeChanged(const mm::gui::Theme& t)
{
    curve.setTheme(t);
    clipMeter.setTheme(t);
    drawer.setTheme(t);
    clipLabel.setColour(juce::Label::textColourId, t.textDim);
}

void SoftClipperEditor::meterFrame(const mm::MeterFrame& f)
{
    const float clip = clipper.currentClipDb();
    clipMeter.setGainReduction(clip);
    curve.setLive(f.peakInDb[0], clip);
    curve.refresh();
}
