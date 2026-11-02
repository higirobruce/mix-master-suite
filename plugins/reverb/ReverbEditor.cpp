#include "plugins/reverb/ReverbEditor.h"
#include "Params.h"
#include "plugins/reverb/ReverbProcessor.h"

namespace rp = mm::plugins::reverb;

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

ReverbEditor::ReverbEditor(ReverbProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::timeSpace), 920, 580), reverb(p),
      algorithm(p.apvts(), rp::id::algorithm, "ALGORITHM"), predelay(p.apvts(), rp::id::predelay_ms, "PRE-DELAY"),
      decay(p.apvts(), rp::id::decay_s, "DECAY"), size(p.apvts(), rp::id::size_pct, "SIZE"), damping(p.apvts(), rp::id::damping_pct, "DAMPING"),
      diffusion(p.apvts(), rp::id::diffusion_pct, "DIFFUSION"), earlyLate(p.apvts(), rp::id::early_late_pct, "EARLY / LATE"),
      mix(p.apvts(), rp::id::mix_pct, "MIX"), drawer(juce::String::fromUTF8("Low cut \xc2\xb7 High cut \xc2\xb7 Width")), lowCut(p.apvts(), rp::id::low_cut_hz, "LOW CUT"),
      highCut(p.apvts(), rp::id::high_cut_hz, "HIGH CUT"), width(p.apvts(), rp::id::width_pct, "WIDTH")
{
    auto& c = content();
    c.addAndMakeVisible(decayDisplay);
    c.addAndMakeVisible(algorithm);
    for (auto* k : {&predelay, &decay, &size, &damping, &diffusion, &earlyLate, &mix})
        c.addAndMakeVisible(*k);
    decayDisplay.onDecayChange = [this](double seconds) { setReal(reverb.apvts(), rp::id::decay_s, static_cast<float>(seconds)); };
    decayDisplay.onGesture = [this](bool starting) {
        if (auto* param = reverb.apvts().getParameter(rp::id::decay_s))
            starting ? param->beginChangeGesture() : param->endChangeGesture();
    };
    for (auto* k : {&lowCut, &highCut, &width})
        drawerContent.addAndMakeVisible(*k);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(92);
    drawer.setOpen(static_cast<bool>(reverb.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { reverb.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    decayDisplay.setTheme(theme());
    drawer.setTheme(theme());
    decayDisplay.setDecay(reverb.apvts().getRawParameterValue(rp::id::decay_s)->load(), reverb.apvts().getRawParameterValue(rp::id::predelay_ms)->load(),
                          reverb.apvts().getRawParameterValue(rp::id::early_late_pct)->load() * 0.01f);
    initialise();
}

void ReverbEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    auto dc = drawerArea.withTrimmedTop(mm::gui::AdvancedDrawer::kHeaderHeight).reduced(0, 2);
    drawerContent.setBounds(dc.withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    {
        auto row = drawerContent.getLocalBounds();
        const int w = row.getWidth() / 3;
        lowCut.setBounds(row.removeFromLeft(w).reduced(4, 0));
        highCut.setBounds(row.removeFromLeft(w).reduced(4, 0));
        width.setBounds(row.reduced(4, 0));
    }
    area.removeFromBottom(6);
    auto strip = area.removeFromBottom(120);
    const int colW = strip.getWidth() / 8;
    {
        auto col = strip.removeFromLeft(colW).reduced(4, 0);
        algorithm.setBounds(col.withTrimmedTop(30).withHeight(40));
    }
    for (auto* k : {&predelay, &decay, &size, &damping, &diffusion, &earlyLate})
        k->setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    mix.setBounds(strip.reduced(4, 0));
    area.removeFromBottom(8);
    decayDisplay.setBounds(area);
}

void ReverbEditor::themeChanged(const mm::gui::Theme& t)
{
    decayDisplay.setTheme(t);
    drawer.setTheme(t);
}

void ReverbEditor::meterFrame(const mm::MeterFrame& f)
{
    decayDisplay.setDecay(reverb.apvts().getRawParameterValue(rp::id::decay_s)->load(), reverb.apvts().getRawParameterValue(rp::id::predelay_ms)->load(),
                          reverb.apvts().getRawParameterValue(rp::id::early_late_pct)->load() * 0.01f);
    decayDisplay.pushLevel(juce::jmax(f.rmsOutDb[0], f.rmsOutDb[1]));
    static const char* names[4] = {"Room", "Hall", "Plate", "Chamber"};
    const int algo = juce::jlimit(0, 3, static_cast<int>(std::lround(reverb.apvts().getRawParameterValue(rp::id::algorithm)->load())));
    footerBar().setStatusSuffix(juce::String(names[algo]) + (algo == 2 ? " (fixed size, decay >= 0.5 s)" : ""));
}
