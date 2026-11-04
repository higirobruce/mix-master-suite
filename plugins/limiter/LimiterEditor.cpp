#include "plugins/limiter/LimiterEditor.h"
#include "Params.h"
#include "plugins/limiter/LimiterProcessor.h"

namespace lim = mm::plugins::limiter;

LimiterEditor::LimiterEditor(LimiterProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::mastering), 920, 580), limiter(p),
      input(p.apvts(), lim::id::input_db, "INPUT"), ceiling(p.apvts(), lim::id::ceiling_dbtp, "CEILING"), release(p.apvts(), lim::id::release_ms, "RELEASE"),
      lookahead(p.apvts(), lim::id::lookahead_ms, "LOOKAHEAD"), autoRelease(p.apvts(), lim::id::auto_release, "AUTO"),
      character(p.apvts(), lim::id::character, "CHARACTER"), drawer("Advanced"), link(p.apvts(), lim::id::link_pct, "LINK"),
      dither(p.apvts(), lim::id::dither, "DITHER")
{
    auto& c = content();
    c.addAndMakeVisible(history);
    c.addAndMakeVisible(readouts);
    c.addAndMakeVisible(resetMeters);
    readouts.setJustificationType(juce::Justification::centredLeft);
    readouts.setFont(juce::Font(juce::FontOptions(15.0f)));
    resetMeters.onClick = [this] {
        limiter.resetMeters();
        history.clear();
    };
    for (auto* comp_ : {static_cast<juce::Component*>(&input), static_cast<juce::Component*>(&ceiling), static_cast<juce::Component*>(&release),
                        static_cast<juce::Component*>(&lookahead), static_cast<juce::Component*>(&autoRelease), static_cast<juce::Component*>(&character)})
        c.addAndMakeVisible(comp_);
    drawerContent.addAndMakeVisible(link);
    drawerContent.addAndMakeVisible(dither);
    drawerContent.addAndMakeVisible(ditherNote);
    ditherNote.setText("Dither is the last stage: leave the footer trim at 0 dB when it is on.", juce::dontSendNotification);
    ditherNote.setJustificationType(juce::Justification::centredLeft);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(110);
    drawer.setOpen(static_cast<bool>(limiter.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { limiter.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    history.setTheme(theme());
    drawer.setTheme(theme());
    readouts.setText(readoutText(), juce::dontSendNotification);
    initialise();
}

juce::String LimiterEditor::readoutText() const
{
    const float lufs = limiter.integratedLufs(), tp = limiter.outputTruePeakDbtp(), lra = limiter.loudnessRangeLu();
    return (lufs > -100.0f ? juce::String(lufs, 1) : juce::String("--")) + " LUFS integrated  \xc2\xb7  "
         + (tp > -100.0f ? juce::String(tp, 1) : juce::String("--")) + " dBTP max  \xc2\xb7  LRA " + juce::String(lra, 1) + " LU";
}

void LimiterEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter, 4);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    drawerContent.setBounds(drawerArea.withZeroOrigin().withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    auto row = drawerContent.getLocalBounds().withHeight(110).reduced(8, 4);
    link.setBounds(row.removeFromLeft(110).reduced(4, 4));
    dither.setBounds(row.removeFromLeft(220).withTrimmedTop(30).withHeight(40).reduced(4, 0));
    ditherNote.setBounds(row.withTrimmedTop(30).withHeight(40).reduced(8, 0));
    area.removeFromBottom(4);
    auto strip = area.removeFromBottom(124);
    const int kw = strip.getWidth() / 6;
    for (auto* k : {&input, &ceiling, &release, &lookahead})
        k->setBounds(strip.removeFromLeft(kw).reduced(4, 0));
    autoRelease.setBounds(strip.removeFromLeft(kw).withTrimmedTop(36).withHeight(44).reduced(4, 0));
    character.setBounds(strip.withTrimmedTop(30).withHeight(40).reduced(4, 0));
    area.removeFromBottom(6);
    auto top = area.removeFromTop(26);
    resetMeters.setBounds(top.removeFromRight(70).reduced(2));
    readouts.setBounds(top);
    history.setBounds(area);
}

void LimiterEditor::themeChanged(const mm::gui::Theme& t)
{
    history.setTheme(t);
    drawer.setTheme(t);
    readouts.setColour(juce::Label::textColourId, t.text);
    ditherNote.setColour(juce::Label::textColourId, t.textDim);
}

void LimiterEditor::meterFrame(const mm::MeterFrame&)
{
    history.push(limiter.currentGainReductionDb(), limiter.overNow());
    readouts.setText(readoutText(), juce::dontSendNotification);
}
