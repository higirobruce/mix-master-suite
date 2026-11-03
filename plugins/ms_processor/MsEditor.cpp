#include "plugins/ms_processor/MsEditor.h"
#include "Params.h"
#include "plugins/ms_processor/MsProcessor.h"

namespace msp = mm::plugins::ms_processor;

namespace
{
const char* kBandIds[4][5] = {
    {msp::id::mid_eq1_enabled, msp::id::mid_eq1_type, msp::id::mid_eq1_freq_hz, msp::id::mid_eq1_gain_db, msp::id::mid_eq1_q},
    {msp::id::mid_eq2_enabled, msp::id::mid_eq2_type, msp::id::mid_eq2_freq_hz, msp::id::mid_eq2_gain_db, msp::id::mid_eq2_q},
    {msp::id::side_eq1_enabled, msp::id::side_eq1_type, msp::id::side_eq1_freq_hz, msp::id::side_eq1_gain_db, msp::id::side_eq1_q},
    {msp::id::side_eq2_enabled, msp::id::side_eq2_type, msp::id::side_eq2_freq_hz, msp::id::side_eq2_gain_db, msp::id::side_eq2_q},
};
const char* kBandTitles[4] = {"MID EQ 1", "MID EQ 2", "SIDE EQ 1", "SIDE EQ 2"};
} // namespace

MsEditor::MsEditor(MsProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::mastering), 920, 580), ms(p),
      spectrum(nullptr, &p.outputAnalyser(), p.getSampleRate() > 0 ? p.getSampleRate() : 48000.0),
      mid(p.apvts(), msp::id::mid_db, "MID"), side(p.apvts(), msp::id::side_db, "SIDE"), width(p.apvts(), msp::id::width_pct, "WIDTH"),
      monoBelow(p.apvts(), msp::id::mono_below_hz, "MONO BELOW"), output(p.apvts(), msp::id::output_db, "OUTPUT"),
      solo(p.apvts(), msp::id::solo, "SOLO"), drawer("Mid / side EQ"), scopeL(2048), scopeR(2048)
{
    auto& c = content();
    c.addAndMakeVisible(gonio);
    spectrum.setVisiblePrePost(false, true);
    c.addAndMakeVisible(spectrum);
    for (auto* comp_ : {static_cast<juce::Component*>(&mid), static_cast<juce::Component*>(&side), static_cast<juce::Component*>(&width),
                        static_cast<juce::Component*>(&monoBelow), static_cast<juce::Component*>(&output), static_cast<juce::Component*>(&solo)})
        c.addAndMakeVisible(comp_);
    for (int b = 0; b < 4; ++b)
    {
        auto& bc = bands[static_cast<size_t>(b)];
        bc.enabled = std::make_unique<mm::gui::Toggle>(p.apvts(), kBandIds[b][0], "ON");
        bc.type = std::make_unique<mm::gui::Selector>(p.apvts(), kBandIds[b][1], "TYPE");
        bc.freq = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][2], "FREQ");
        bc.gain = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][3], "GAIN");
        bc.q = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][4], "Q");
        bc.title.setText(kBandTitles[b], juce::dontSendNotification);
        bc.title.setJustificationType(juce::Justification::centredLeft);
        for (juce::Component* comp_ : {static_cast<juce::Component*>(bc.enabled.get()), static_cast<juce::Component*>(bc.type.get()),
                                       static_cast<juce::Component*>(bc.freq.get()), static_cast<juce::Component*>(bc.gain.get()),
                                       static_cast<juce::Component*>(bc.q.get()), static_cast<juce::Component*>(&bc.title)})
            drawerContent.addAndMakeVisible(comp_);
    }
    drawer.setContent(drawerContent);
    drawer.setContentHeight(150);
    drawer.setOpen(static_cast<bool>(ms.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { ms.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    gonio.setTheme(theme());
    spectrum.setTheme(theme());
    drawer.setTheme(theme());
    initialise();
}

void MsEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter, 4);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    drawerContent.setBounds(drawerArea.withZeroOrigin().withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    auto row = drawerContent.getLocalBounds().withHeight(150).reduced(6, 2);
    const int colW = row.getWidth() / 4;
    for (auto& bc : bands)
    {
        auto col = row.removeFromLeft(colW).reduced(4, 0);
        bc.title.setBounds(col.removeFromTop(16));
        auto top = col.removeFromTop(30);
        bc.enabled->setBounds(top.removeFromLeft(60));
        bc.type->setBounds(top.reduced(4, 0));
        auto knobs = col.removeFromTop(90);
        const int kw = knobs.getWidth() / 3;
        bc.freq->setBounds(knobs.removeFromLeft(kw));
        bc.gain->setBounds(knobs.removeFromLeft(kw));
        bc.q->setBounds(knobs);
    }
    area.removeFromBottom(4);
    auto strip = area.removeFromBottom(120);
    const int kw = strip.getWidth() / 6;
    for (auto* k : {&mid, &side, &width, &monoBelow, &output})
        k->setBounds(strip.removeFromLeft(kw).reduced(4, 0));
    solo.setBounds(strip.reduced(4, 0).withTrimmedTop(30).withHeight(40));
    area.removeFromBottom(6);
    auto spec = area.removeFromRight(area.getWidth() * 2 / 5);
    area.removeFromRight(6);
    gonio.setBounds(area);
    spectrum.setBounds(spec);
}

void MsEditor::themeChanged(const mm::gui::Theme& t)
{
    gonio.setTheme(t);
    spectrum.setTheme(t);
    drawer.setTheme(t);
    for (auto& bc : bands)
        bc.title.setColour(juce::Label::textColourId, t.textDim);
}

void MsEditor::meterFrame(const mm::MeterFrame&)
{
    const int n = ms.pullScope(scopeL.data(), scopeR.data(), 2048);
    gonio.pushScope(scopeL.data(), scopeR.data(), n);
    const float corr = ms.currentCorrelation();
    gonio.setCorrelation(corr);
    spectrum.setSampleRate(ms.getSampleRate() > 0 ? ms.getSampleRate() : 48000.0);
    spectrum.pull();
    const float mono = ms.apvts().getRawParameterValue(msp::id::mono_below_hz)->load();
    spectrum.setMarkerHz(mono >= 5.0f ? mono : 0.0f);
    footerBar().setStatusSuffix("correlation " + juce::String(corr, 2));
}
