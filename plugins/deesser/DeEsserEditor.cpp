#include "plugins/deesser/DeEsserEditor.h"
#include "Params.h"
#include "plugins/deesser/DeEsserProcessor.h"

namespace dp = mm::plugins::deesser;

DeEsserEditor::DeEsserEditor(DeEsserProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::dynamics), 920, 580), deesser(p),
      freq(p.apvts(), dp::id::freq_hz, "FREQUENCY"), threshold(p.apvts(), dp::id::threshold_db, "THRESHOLD"), range(p.apvts(), dp::id::range_db, "RANGE"),
      width(p.apvts(), dp::id::width_oct, "WIDTH"), lookahead(p.apvts(), dp::id::lookahead_ms, "LOOKAHEAD"), mode(p.apvts(), dp::id::mode, "MODE"),
      listen(p.apvts(), dp::id::listen, "LISTEN (what is removed)")
{
    auto& c = content();
    c.addAndMakeVisible(history);
    c.addAndMakeVisible(grMeter);
    c.addAndMakeVisible(grLabel);
    grLabel.setText("GR", juce::dontSendNotification);
    grLabel.setJustificationType(juce::Justification::centred);
    grLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    for (auto* k : {&freq, &threshold, &range, &width, &lookahead})
        c.addAndMakeVisible(*k);
    c.addAndMakeVisible(mode);
    c.addAndMakeVisible(listen);
    history.setTheme(theme());
    grMeter.setTheme(theme());
    initialise();
}

void DeEsserEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto strip = area.removeFromBottom(120);
    const int colW = strip.getWidth() / 7;
    for (auto* k : {&freq, &threshold, &range, &width})
        k->setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    {
        auto col = strip.removeFromLeft(colW).reduced(4, 0);
        mode.setBounds(col.withTrimmedTop(30).withHeight(40));
    }
    lookahead.setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    listen.setBounds(strip.reduced(4, 0).withTrimmedTop(36).withHeight(44));
    area.removeFromBottom(8);
    auto meterCol = area.removeFromRight(28);
    grLabel.setBounds(meterCol.removeFromTop(16));
    grMeter.setBounds(meterCol.reduced(4, 0));
    area.removeFromRight(6);
    history.setBounds(area);
}

void DeEsserEditor::themeChanged(const mm::gui::Theme& t)
{
    history.setTheme(t);
    grMeter.setTheme(t);
    grLabel.setColour(juce::Label::textColourId, t.textDim);
}

void DeEsserEditor::meterFrame(const mm::MeterFrame& f)
{
    history.push(f, deesser.sibilantNow(), f.gainReductionDb);
    grMeter.setGainReduction(f.gainReductionDb);
    const bool split = deesser.apvts().getRawParameterValue(dp::id::mode)->load() >= 0.5f;
    const float la = deesser.apvts().getRawParameterValue(dp::id::lookahead_ms)->load();
    footerBar().setStatusSuffix(juce::String(split ? "Split" : "Band") + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(la, 1) + " ms lookahead");
}
