#include "plugins/multiband_comp/MultibandCompEditor.h"
#include "Params.h"
#include "plugins/multiband_comp/MultibandCompProcessor.h"

namespace mp = mm::plugins::multiband_comp;

namespace
{
const char* kBandIds[5][7] = {
    {mp::id::b1_threshold_db, mp::id::b1_ratio, mp::id::b1_attack_ms, mp::id::b1_release_ms, mp::id::b1_gain_db, mp::id::b1_solo, mp::id::b1_bypass},
    {mp::id::b2_threshold_db, mp::id::b2_ratio, mp::id::b2_attack_ms, mp::id::b2_release_ms, mp::id::b2_gain_db, mp::id::b2_solo, mp::id::b2_bypass},
    {mp::id::b3_threshold_db, mp::id::b3_ratio, mp::id::b3_attack_ms, mp::id::b3_release_ms, mp::id::b3_gain_db, mp::id::b3_solo, mp::id::b3_bypass},
    {mp::id::b4_threshold_db, mp::id::b4_ratio, mp::id::b4_attack_ms, mp::id::b4_release_ms, mp::id::b4_gain_db, mp::id::b4_solo, mp::id::b4_bypass},
    {mp::id::b5_threshold_db, mp::id::b5_ratio, mp::id::b5_attack_ms, mp::id::b5_release_ms, mp::id::b5_gain_db, mp::id::b5_solo, mp::id::b5_bypass},
};
const char* kXoverIds[4] = {mp::id::xover_1_hz, mp::id::xover_2_hz, mp::id::xover_3_hz, mp::id::xover_4_hz};

void setReal(juce::AudioProcessorValueTreeState& apvts, const char* id, double value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        const auto& r = p->getNormalisableRange();
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(r.start, r.end, static_cast<float>(value))));
    }
}
} // namespace

MultibandCompEditor::MultibandCompEditor(MultibandCompProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::mastering), 920, 580), comp(p),
      display(&p.postAnalyser(), p.getSampleRate() > 0 ? p.getSampleRate() : 48000.0),
      numBands(p.apvts(), mp::id::num_bands, "BANDS"), mix(p.apvts(), mp::id::mix_pct, "MIX"), drawer("Advanced"),
      detector(p.apvts(), mp::id::detector, "DETECTOR"), scExternal(p.apvts(), mp::id::sc_external, "EXT KEY")
{
    auto& c = content();
    c.addAndMakeVisible(display);
    display.onCrossoverDrag = [this](int i, double hz) { setReal(comp.apvts(), kXoverIds[i], hz); };
    display.onCrossoverGesture = [this](int i, bool starting) {
        auto* par = comp.apvts().getParameter(kXoverIds[i]);
        if (starting) par->beginChangeGesture(); else par->endChangeGesture();
    };
    display.onFocusBand = [this](int band) { setFocused(band); };
    for (int b = 0; b < 5; ++b)
    {
        auto& bc = bands[static_cast<size_t>(b)];
        bc.threshold = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][0], "THRESHOLD");
        bc.ratio = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][1], "RATIO");
        bc.attack = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][2], "ATTACK");
        bc.release = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][3], "RELEASE");
        bc.gain = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[b][4], "GAIN");
        bc.solo = std::make_unique<mm::gui::Toggle>(p.apvts(), kBandIds[b][5], "SOLO");
        bc.bypass = std::make_unique<mm::gui::Toggle>(p.apvts(), kBandIds[b][6], "BYPASS");
        for (juce::Component* comp_ : {static_cast<juce::Component*>(bc.threshold.get()), static_cast<juce::Component*>(bc.ratio.get()),
                                       static_cast<juce::Component*>(bc.attack.get()), static_cast<juce::Component*>(bc.release.get()),
                                       static_cast<juce::Component*>(bc.gain.get()), static_cast<juce::Component*>(bc.solo.get()),
                                       static_cast<juce::Component*>(bc.bypass.get())})
            c.addChildComponent(comp_); // shown for the focused band only
    }
    for (int i = 0; i < 4; ++i)
    {
        crossovers[static_cast<size_t>(i)] = std::make_unique<mm::gui::Knob>(p.apvts(), kXoverIds[i], "X" + juce::String(i + 1));
        c.addAndMakeVisible(*crossovers[static_cast<size_t>(i)]);
    }
    c.addAndMakeVisible(numBands);
    c.addAndMakeVisible(mix);
    c.addAndMakeVisible(bandTitle);
    bandTitle.setJustificationType(juce::Justification::centredLeft);
    drawerContent.addAndMakeVisible(detector);
    drawerContent.addAndMakeVisible(scExternal);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(44);
    drawer.setOpen(static_cast<bool>(comp.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { comp.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    display.setTheme(theme());
    drawer.setTheme(theme());
    for (int i = 0; i < 4; ++i)
        crossovers[static_cast<size_t>(i)]->setVisible(i < comp.numBands() - 1);
    setFocused(static_cast<int>(comp.uiState().getProperty("focusedBand", 0)));
    initialise();
}

void MultibandCompEditor::setFocused(int band)
{
    focused = juce::jlimit(0, comp.numBands() - 1, band);
    comp.uiState().setProperty("focusedBand", focused, nullptr);
    for (int b = 0; b < 5; ++b)
    {
        auto& bc = bands[static_cast<size_t>(b)];
        const bool show = b == focused;
        for (juce::Component* comp_ : {static_cast<juce::Component*>(bc.threshold.get()), static_cast<juce::Component*>(bc.ratio.get()),
                                       static_cast<juce::Component*>(bc.attack.get()), static_cast<juce::Component*>(bc.release.get()),
                                       static_cast<juce::Component*>(bc.gain.get()), static_cast<juce::Component*>(bc.solo.get()),
                                       static_cast<juce::Component*>(bc.bypass.get())})
            comp_->setVisible(show);
    }
    bandTitle.setText("BAND " + juce::String(focused + 1), juce::dontSendNotification);
    if (display.focusedBand() != focused)
        display.focusBand(focused);
    resized();
}

void MultibandCompEditor::layoutBand(BandControls& b, juce::Rectangle<int> area)
{
    const int kw = area.getWidth() / 7;
    b.threshold->setBounds(area.removeFromLeft(kw).reduced(4, 0));
    b.ratio->setBounds(area.removeFromLeft(kw).reduced(4, 0));
    b.attack->setBounds(area.removeFromLeft(kw).reduced(4, 0));
    b.release->setBounds(area.removeFromLeft(kw).reduced(4, 0));
    b.gain->setBounds(area.removeFromLeft(kw).reduced(4, 0));
    auto toggles = area.reduced(4, 0);
    b.solo->setBounds(toggles.removeFromTop(toggles.getHeight() / 2).reduced(0, 10));
    b.bypass->setBounds(toggles.reduced(0, 10));
}

void MultibandCompEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter, 4);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    drawerContent.setBounds(drawerArea.withZeroOrigin().withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    auto row = drawerContent.getLocalBounds().withHeight(44).reduced(8, 4);
    detector.setBounds(row.removeFromLeft(220).reduced(4, 0));
    scExternal.setBounds(row.removeFromLeft(160).reduced(4, 4));
    area.removeFromBottom(4);
    auto strip = area.removeFromBottom(124);
    auto left = strip.removeFromLeft(strip.getWidth() * 5 / 12);
    numBands.setBounds(left.removeFromLeft(90).withTrimmedTop(28).withHeight(40).reduced(4, 0));
    const int xw = (left.getWidth() - 70) / 4;
    for (auto& k : crossovers)
        k->setBounds(left.removeFromLeft(xw).reduced(2, 0));
    mix.setBounds(left.reduced(2, 0));
    bandTitle.setBounds(strip.removeFromTop(18));
    for (auto& b : bands)
        layoutBand(b, strip);
    area.removeFromBottom(6);
    display.setBounds(area);
}

void MultibandCompEditor::themeChanged(const mm::gui::Theme& t)
{
    display.setTheme(t);
    drawer.setTheme(t);
    bandTitle.setColour(juce::Label::textColourId, t.textDim);
}

void MultibandCompEditor::meterFrame(const mm::MeterFrame&)
{
    display.setSampleRate(comp.getSampleRate() > 0 ? comp.getSampleRate() : 48000.0);
    display.pull();
    const int n = comp.numBands();
    double xo[4];
    for (int i = 0; i < 4; ++i)
        xo[i] = i < n - 1 ? comp.crossoverHz(i) : 0.0;
    if (xo[0] <= 0.0) // before the first block: the parameters themselves
        for (int i = 0; i < n - 1; ++i)
            xo[i] = comp.apvts().getRawParameterValue(kXoverIds[i])->load();
    display.setBands(n, xo);
    for (int b = 0; b < 5; ++b)
        display.setBandReduction(b, comp.bandReductionDb(b), comp.bandLevelDb(b));
    for (int i = 0; i < 4; ++i)
        crossovers[static_cast<size_t>(i)]->setVisible(i < n - 1);
    if (focused >= n)
        setFocused(n - 1);
}
