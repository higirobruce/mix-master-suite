#include "plugins/channel_comp/ChannelCompEditor.h"
#include "Params.h"
#include "plugins/channel_comp/ChannelCompProcessor.h"

namespace cp = mm::plugins::channel_comp;

namespace
{
const char* const kCurveParams[] = {cp::id::threshold_db, cp::id::ratio, cp::id::ratio_inf, cp::id::knee_db};

void setReal(juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        const auto& r = p->getNormalisableRange();
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(r.start, r.end, value)));
    }
}
} // namespace

ChannelCompEditor::ChannelCompEditor(ChannelCompProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::dynamics), 920, 580), comp(p),
      curve([this](float levelDb) { return comp.staticGainDb(levelDb); }), threshold(p.apvts(), cp::id::threshold_db, "THRESHOLD"),
      ratio(p.apvts(), cp::id::ratio, "RATIO"), attack(p.apvts(), cp::id::attack_ms, "ATTACK"),
      release(p.apvts(), cp::id::release_ms, "RELEASE"), knee(p.apvts(), cp::id::knee_db, "KNEE"), mix(p.apvts(), cp::id::mix_pct, "MIX"),
      makeup(p.apvts(), cp::id::makeup_db, "MAKEUP"), ratioInf(p.apvts(), cp::id::ratio_inf, "INF"),
      autoRelease(p.apvts(), cp::id::auto_release, "AUTO"), autoMakeup(p.apvts(), cp::id::auto_makeup, "AUTO"),
      detector(p.apvts(), cp::id::detector, "DETECTOR"), drawer("Sidechain & routing"), topology(p.apvts(), cp::id::topology, "TOPOLOGY"),
      scHpEnabled(p.apvts(), cp::id::sc_hp_enabled, "SC HP"), extKey(p.apvts(), cp::id::ext_key, "EXT KEY"),
      listenSc(p.apvts(), cp::id::listen_sc, "LISTEN SC"), scHpHz(p.apvts(), cp::id::sc_hp_hz, "SC HP FREQ"),
      stereoLink(p.apvts(), cp::id::stereo_link_pct, "LINK")
{
    auto& c = content();
    c.addAndMakeVisible(history);
    c.addAndMakeVisible(curve);
    c.addAndMakeVisible(grMeter);
    c.addAndMakeVisible(grLabel);
    grLabel.setText("GR", juce::dontSendNotification);
    grLabel.setJustificationType(juce::Justification::centred);
    grLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    history.setThresholdDb(p.apvts().getRawParameterValue(cp::id::threshold_db)->load());
    history.onThresholdChange = [this](float db) { setReal(comp.apvts(), cp::id::threshold_db, db); };
    history.onGesture = [this](bool starting) {
        if (auto* param = comp.apvts().getParameter(cp::id::threshold_db))
            starting ? param->beginChangeGesture() : param->endChangeGesture();
    };
    for (auto* k : {&threshold, &ratio, &attack, &release, &knee, &mix, &makeup})
        c.addAndMakeVisible(*k);
    for (auto* t : {&ratioInf, &autoRelease, &autoMakeup})
        c.addAndMakeVisible(*t);
    c.addAndMakeVisible(detector);

    drawerContent.addAndMakeVisible(topology);
    drawerContent.addAndMakeVisible(scHpEnabled);
    drawerContent.addAndMakeVisible(scHpHz);
    drawerContent.addAndMakeVisible(extKey);
    drawerContent.addAndMakeVisible(stereoLink);
    drawerContent.addAndMakeVisible(listenSc);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(84);
    drawer.setOpen(static_cast<bool>(comp.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { comp.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);

    for (auto* id : kCurveParams)
        comp.apvts().addParameterListener(id, this);

    history.setTheme(theme());
    curve.setTheme(theme());
    grMeter.setTheme(theme());
    drawer.setTheme(theme());
    initialise();
}

ChannelCompEditor::~ChannelCompEditor()
{
    for (auto* id : kCurveParams)
        comp.apvts().removeParameterListener(id, this);
}

void ChannelCompEditor::parameterChanged(const juce::String&, float)
{
    curveDirty.store(true, std::memory_order_relaxed); // may arrive on the audio thread (host automation)
}

void ChannelCompEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    auto dc = drawerArea.withTrimmedTop(mm::gui::AdvancedDrawer::kHeaderHeight).reduced(0, 2);
    drawerContent.setBounds(dc.withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    {
        auto row = drawerContent.getLocalBounds();
        const int w = row.getWidth() / 6;
        auto topRow = [&](juce::Component& comp_, juce::Rectangle<int> col) { comp_.setBounds(col.reduced(6, 0).removeFromTop(40)); };
        topRow(topology, row.removeFromLeft(w));
        topRow(scHpEnabled, row.removeFromLeft(w).withTrimmedTop(16).withHeight(24));
        scHpHz.setBounds(row.removeFromLeft(w).reduced(6, 0));
        topRow(extKey, row.removeFromLeft(w).withTrimmedTop(16).withHeight(24));
        stereoLink.setBounds(row.removeFromLeft(w).reduced(6, 0));
        topRow(listenSc, row.withTrimmedTop(16).withHeight(24));
    }
    area.removeFromBottom(6);

    auto strip = area.removeFromBottom(120);
    const int colW = strip.getWidth() / 8;
    auto knobWithToggle = [&](mm::gui::Knob& k, mm::gui::Toggle* t, juce::Rectangle<int> col) {
        col = col.reduced(4, 0);
        if (t != nullptr)
            t->setBounds(col.removeFromBottom(20).reduced(10, 0));
        k.setBounds(col);
    };
    knobWithToggle(threshold, nullptr, strip.removeFromLeft(colW));
    knobWithToggle(ratio, &ratioInf, strip.removeFromLeft(colW));
    knobWithToggle(attack, nullptr, strip.removeFromLeft(colW));
    knobWithToggle(release, &autoRelease, strip.removeFromLeft(colW));
    knobWithToggle(knee, nullptr, strip.removeFromLeft(colW));
    {
        auto col = strip.removeFromLeft(colW).reduced(4, 0);
        detector.setBounds(col.withTrimmedTop(30).withHeight(40));
    }
    knobWithToggle(mix, nullptr, strip.removeFromLeft(colW));
    knobWithToggle(makeup, &autoMakeup, strip);
    area.removeFromBottom(8);

    auto meterCol = area.removeFromRight(28);
    grLabel.setBounds(meterCol.removeFromTop(16));
    grMeter.setBounds(meterCol.reduced(4, 0));
    area.removeFromRight(6);
    curve.setBounds(area.removeFromRight(area.getHeight()));
    area.removeFromRight(6);
    history.setBounds(area);
}

void ChannelCompEditor::themeChanged(const mm::gui::Theme& t)
{
    history.setTheme(t);
    curve.setTheme(t);
    grMeter.setTheme(t);
    drawer.setTheme(t);
    grLabel.setColour(juce::Label::textColourId, t.textDim);
}

void ChannelCompEditor::meterFrame(const mm::MeterFrame& f)
{
    history.push(f);
    history.setThresholdDb(comp.apvts().getRawParameterValue(cp::id::threshold_db)->load());
    grMeter.setGainReduction(f.gainReductionDb);
    curve.setLive(juce::jmax(f.peakInDb[0], f.peakInDb[1]), f.gainReductionDb);
    if (curveDirty.exchange(false, std::memory_order_relaxed))
        curve.refresh();
    const int topo = static_cast<int>(comp.apvts().getRawParameterValue(cp::id::topology)->load());
    const bool ext = comp.apvts().getRawParameterValue(cp::id::ext_key)->load() >= 0.5f;
    juce::String status(topo == 0 ? "FF" : "FB");
    if (topo == 1)
    {
        const bool inf = comp.apvts().getRawParameterValue(cp::id::ratio_inf)->load() >= 0.5f;
        const float label = inf ? 1000.0f : comp.apvts().getRawParameterValue(cp::id::ratio)->load();
        const float eff = comp.effectiveRatio();
        if (eff < label - 0.05f)
            status << juce::String::fromUTF8(" \xc2\xb7 loop limits ratio to ") << juce::String(eff, 1) << ":1";
    }
    footerBar().setStatusSuffix(status + (ext ? " / ext key" : ""));
}
