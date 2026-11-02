#include "plugins/delay/DelayEditor.h"
#include "Params.h"
#include "plugins/delay/DelayProcessor.h"

namespace dp = mm::plugins::delay;

namespace
{
void setReal(juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        const auto& r = p->getNormalisableRange();
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(r.start, r.end, value)));
    }
}
} // namespace

DelayEditor::DelayEditor(DelayProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::timeSpace), 920, 580), delay(p),
      timeL(p.apvts(), dp::id::time_l_ms, "TIME L"), timeR(p.apvts(), dp::id::time_r_ms, "TIME R"),
      feedback(p.apvts(), dp::id::feedback_pct, "FEEDBACK"), mix(p.apvts(), dp::id::mix_pct, "MIX"), link(p.apvts(), dp::id::link_lr, "LINK"),
      pingPong(p.apvts(), dp::id::ping_pong, "PING-PONG"), sync(p.apvts(), dp::id::sync, "SYNC"), drawer(juce::String::fromUTF8("Filter \xc2\xb7 Mod \xc2\xb7 Duck")),
      fbLp(p.apvts(), dp::id::fb_lp_hz, "FB LOW-PASS"), fbHp(p.apvts(), dp::id::fb_hp_hz, "FB HIGH-PASS"),
      modDepth(p.apvts(), dp::id::mod_depth_ms, "MOD DEPTH"), modRate(p.apvts(), dp::id::mod_rate_hz, "MOD RATE"),
      duckAmount(p.apvts(), dp::id::duck_amount_db, "DUCK"), duckRelease(p.apvts(), dp::id::duck_release_ms, "DUCK RELEASE")
{
    auto& c = content();
    c.addAndMakeVisible(taps);
    for (auto* k : {&timeL, &timeR, &feedback, &mix})
        c.addAndMakeVisible(*k);
    c.addAndMakeVisible(link);
    c.addAndMakeVisible(pingPong);
    c.addAndMakeVisible(sync);
    taps.onTimeChange = [this](int channel, double ms) {
        setReal(delay.apvts(), channel == 0 ? dp::id::time_l_ms : dp::id::time_r_ms, static_cast<float>(ms));
    };
    taps.onGesture = [this](int channel, bool starting) {
        if (auto* param = delay.apvts().getParameter(channel == 0 ? dp::id::time_l_ms : dp::id::time_r_ms))
            starting ? param->beginChangeGesture() : param->endChangeGesture();
    };
    for (auto* k : {&fbLp, &fbHp, &modDepth, &modRate, &duckAmount, &duckRelease})
        drawerContent.addAndMakeVisible(*k);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(92);
    drawer.setOpen(static_cast<bool>(delay.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { delay.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    taps.setTheme(theme());
    drawer.setTheme(theme());
    taps.setBpm(delay.currentBpm());
    initialise();
}

void DelayEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    auto dc = drawerArea.withTrimmedTop(mm::gui::AdvancedDrawer::kHeaderHeight).reduced(0, 2);
    drawerContent.setBounds(dc.withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    {
        auto row = drawerContent.getLocalBounds();
        const int w = row.getWidth() / 6;
        for (auto* k : {&fbLp, &fbHp, &modDepth, &modRate, &duckAmount})
            k->setBounds(row.removeFromLeft(w).reduced(4, 0));
        duckRelease.setBounds(row.reduced(4, 0));
    }
    area.removeFromBottom(6);
    auto strip = area.removeFromBottom(120);
    const int colW = strip.getWidth() / 6;
    timeL.setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    timeR.setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    {
        auto col = strip.removeFromLeft(colW).reduced(4, 0);
        link.setBounds(col.removeFromTop(24).reduced(8, 0));
        col.removeFromTop(6);
        sync.setBounds(col.removeFromTop(40));
        col.removeFromTop(6);
        pingPong.setBounds(col.removeFromTop(24).reduced(8, 0));
    }
    feedback.setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    strip.removeFromLeft(colW); // breathing room
    mix.setBounds(strip.reduced(4, 0));
    area.removeFromBottom(8);
    taps.setBounds(area);
}

void DelayEditor::themeChanged(const mm::gui::Theme& t)
{
    taps.setTheme(t);
    drawer.setTheme(t);
}

void DelayEditor::meterFrame(const mm::MeterFrame&)
{
    const bool synced = delay.apvts().getRawParameterValue(dp::id::sync)->load() >= 0.5f;
    taps.setBpm(delay.currentBpm());
    taps.setTaps(delay.effectiveTimeMs(0), delay.effectiveTimeMs(1), delay.apvts().getRawParameterValue(dp::id::feedback_pct)->load() * 0.01f,
                 delay.apvts().getRawParameterValue(dp::id::ping_pong)->load() >= 0.5f, synced);
    footerBar().setStatusSuffix(juce::String(delay.currentBpm(), 1) + " BPM" + (synced ? " / synced" : ""));
}
