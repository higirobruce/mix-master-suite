#include "plugins/gate/GateEditor.h"
#include "Params.h"
#include "plugins/gate/GateProcessor.h"

namespace gp = mm::plugins::gate;

namespace
{
const char* const kCurveParams[] = {gp::id::threshold_db, gp::id::range_db, gp::id::ratio, gp::id::mode};

void setReal(juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        const auto& r = p->getNormalisableRange();
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(r.start, r.end, value)));
    }
}
} // namespace

GateEditor::GateEditor(GateProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::dynamics), 920, 580), gate(p),
      curve([this](float levelDb) { return gate.staticGainDb(levelDb); }, -80.0f, 0.0f),
      threshold(p.apvts(), gp::id::threshold_db, "THRESHOLD"), range(p.apvts(), gp::id::range_db, "RANGE"),
      attack(p.apvts(), gp::id::attack_ms, "ATTACK"), hold(p.apvts(), gp::id::hold_ms, "HOLD"),
      release(p.apvts(), gp::id::release_ms, "RELEASE"), ratio(p.apvts(), gp::id::ratio, "RATIO"),
      hysteresis(p.apvts(), gp::id::hysteresis_db, "HYSTERESIS"), lookahead(p.apvts(), gp::id::lookahead_ms, "LOOKAHEAD"),
      mode(p.apvts(), gp::id::mode, "MODE"), drawer("Sidechain & routing"), scHpEnabled(p.apvts(), gp::id::sc_hp_enabled, "SC HP"),
      scLpEnabled(p.apvts(), gp::id::sc_lp_enabled, "SC LP"), extKey(p.apvts(), gp::id::ext_key, "EXT KEY"),
      listenSc(p.apvts(), gp::id::listen_sc, "LISTEN"), scHpHz(p.apvts(), gp::id::sc_hp_hz, "HP FREQ"),
      scLpHz(p.apvts(), gp::id::sc_lp_hz, "LP FREQ")
{
    auto& c = content();
    history.setShadeClosed(true);
    c.addAndMakeVisible(history);
    c.addAndMakeVisible(curve);
    c.addAndMakeVisible(grMeter);
    c.addAndMakeVisible(grLabel);
    c.addAndMakeVisible(stateLabel);
    grLabel.setText("GR", juce::dontSendNotification);
    grLabel.setJustificationType(juce::Justification::centred);
    grLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    stateLabel.setText("CLOSED", juce::dontSendNotification);
    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    history.setThresholdDb(p.apvts().getRawParameterValue(gp::id::threshold_db)->load());
    history.onThresholdChange = [this](float db) { setReal(gate.apvts(), gp::id::threshold_db, db); };
    for (auto* k : {&threshold, &range, &attack, &hold, &release, &ratio, &hysteresis, &lookahead})
        c.addAndMakeVisible(*k);
    c.addAndMakeVisible(mode);

    for (auto* comp_ : std::initializer_list<juce::Component*>{&scHpEnabled, &scHpHz, &scLpEnabled, &scLpHz, &extKey, &listenSc})
        drawerContent.addAndMakeVisible(*comp_);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(84);
    drawer.setOpen(static_cast<bool>(gate.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { gate.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);

    for (auto* id : kCurveParams)
        gate.apvts().addParameterListener(id, this);

    history.setTheme(theme());
    curve.setTheme(theme());
    grMeter.setTheme(theme());
    drawer.setTheme(theme());
    initialise();
}

GateEditor::~GateEditor()
{
    for (auto* id : kCurveParams)
        gate.apvts().removeParameterListener(id, this);
}

void GateEditor::parameterChanged(const juce::String&, float)
{
    curveDirty.store(true, std::memory_order_relaxed); // may arrive on the audio thread (host automation)
}

void GateEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    auto dc = drawerArea.withTrimmedTop(mm::gui::AdvancedDrawer::kHeaderHeight).reduced(0, 2);
    drawerContent.setBounds(dc.withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    {
        auto row = drawerContent.getLocalBounds();
        const int w = row.getWidth() / 6;
        auto toggleRow = [&](juce::Component& comp_, juce::Rectangle<int> col) { comp_.setBounds(col.reduced(6, 0).withTrimmedTop(16).withHeight(24)); };
        toggleRow(scHpEnabled, row.removeFromLeft(w));
        scHpHz.setBounds(row.removeFromLeft(w).reduced(6, 0));
        toggleRow(scLpEnabled, row.removeFromLeft(w));
        scLpHz.setBounds(row.removeFromLeft(w).reduced(6, 0));
        toggleRow(extKey, row.removeFromLeft(w));
        toggleRow(listenSc, row);
    }
    area.removeFromBottom(6);

    auto strip = area.removeFromBottom(120);
    const int colW = strip.getWidth() / 9;
    for (auto* k : {&threshold, &range, &attack, &hold, &release})
        k->setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    {
        auto col = strip.removeFromLeft(colW).reduced(4, 0);
        mode.setBounds(col.withTrimmedTop(30).withHeight(40));
    }
    for (auto* k : {&ratio, &hysteresis})
        k->setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    lookahead.setBounds(strip.reduced(4, 0));
    area.removeFromBottom(8);

    auto meterCol = area.removeFromRight(48);
    grLabel.setBounds(meterCol.removeFromTop(16));
    stateLabel.setBounds(meterCol.removeFromBottom(16));
    grMeter.setBounds(meterCol.reduced(14, 2));
    area.removeFromRight(6);
    curve.setBounds(area.removeFromRight(area.getHeight()));
    area.removeFromRight(6);
    history.setBounds(area);
}

void GateEditor::themeChanged(const mm::gui::Theme& t)
{
    history.setTheme(t);
    curve.setTheme(t);
    grMeter.setTheme(t);
    drawer.setTheme(t);
    grLabel.setColour(juce::Label::textColourId, t.textDim);
    stateLabel.setColour(juce::Label::textColourId, t.accent);
}

void GateEditor::meterFrame(const mm::MeterFrame& f)
{
    const bool open = gate.isOpen();
    history.push(f, open);
    history.setThresholdDb(gate.apvts().getRawParameterValue(gp::id::threshold_db)->load());
    grMeter.setGainReduction(f.gainReductionDb);
    curve.setLive(juce::jmax(f.peakInDb[0], f.peakInDb[1]), f.gainReductionDb);
    if (curveDirty.exchange(false, std::memory_order_relaxed))
        curve.refresh();
    stateLabel.setText(open ? "OPEN" : "CLOSED", juce::dontSendNotification);
    const bool ext = gate.apvts().getRawParameterValue(gp::id::ext_key)->load() >= 0.5f;
    const float la = gate.apvts().getRawParameterValue(gp::id::lookahead_ms)->load();
    footerBar().setStatusSuffix(juce::String(la, 1) + " ms lookahead" + (ext ? " / ext key" : ""));
}
