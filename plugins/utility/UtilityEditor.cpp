#include "plugins/utility/UtilityEditor.h"
#include "Params.h"
#include "plugins/utility/UtilityProcessor.h"

namespace up = mm::plugins::utility;

UtilityEditor::MeterDisplay::MeterDisplay()
{
    for (auto* m : {&inL, &inR, &outL, &outR})
        addAndMakeVisible(*m);
    for (auto* l : {&inLabel, &outLabel, &corr})
    {
        l->setJustificationType(juce::Justification::centred);
        l->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(*l);
    }
}

void UtilityEditor::MeterDisplay::setTheme(const mm::gui::Theme& t)
{
    theme = t;
    for (auto* m : {&inL, &inR, &outL, &outR})
        m->setTheme(t);
    for (auto* l : {&inLabel, &outLabel, &corr})
        l->setColour(juce::Label::textColourId, t.textDim);
    repaint();
}

void UtilityEditor::MeterDisplay::update(const mm::MeterFrame& f)
{
    inL.setLevels(f.peakInDb[0], f.rmsInDb[0]);
    inR.setLevels(f.peakInDb[1], f.rmsInDb[1]);
    outL.setLevels(f.peakOutDb[0], f.rmsOutDb[0]);
    outR.setLevels(f.peakOutDb[1], f.rmsOutDb[1]);
}

void UtilityEditor::MeterDisplay::resized()
{
    auto area = getLocalBounds().reduced(24, 16);
    auto in = area.removeFromLeft(area.getWidth() / 2).reduced(30, 0);
    auto out = area.reduced(30, 0);
    inLabel.setBounds(in.removeFromTop(18));
    outLabel.setBounds(out.removeFromTop(18));
    in.removeFromTop(6);
    out.removeFromTop(6);
    const int barW = 22;
    auto inBars = in.withSizeKeepingCentre(barW * 2 + 6, in.getHeight());
    inL.setBounds(inBars.removeFromLeft(barW));
    inBars.removeFromLeft(6);
    inR.setBounds(inBars.removeFromLeft(barW));
    auto outBars = out.withSizeKeepingCentre(barW * 2 + 6, out.getHeight());
    outL.setBounds(outBars.removeFromLeft(barW));
    outBars.removeFromLeft(6);
    outR.setBounds(outBars.removeFromLeft(barW));
}

void UtilityEditor::MeterDisplay::paint(juce::Graphics& g)
{
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), theme.cornerRadius);
    g.setColour(theme.textDim);
    g.setFont(juce::FontOptions(10.0f));
    const auto bars = inL.getBounds(); // scale labels line up with the bars
    for (float db : {0.0f, -6.0f, -18.0f, -36.0f})
    {
        const float y = static_cast<float>(bars.getY()) + mm::gui::MeterBar::dbToY(db, static_cast<float>(bars.getHeight()));
        g.drawText(juce::String(static_cast<int>(db)), bars.getX() - 36, static_cast<int>(y) - 6, 28, 12,
                   juce::Justification::centredRight);
        g.drawText(juce::String(static_cast<int>(db)), outL.getX() - 36, static_cast<int>(y) - 6, 28, 12,
                   juce::Justification::centredRight);
    }
}

UtilityEditor::UtilityEditor(UtilityProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::utility), 920, 580),
      gain(p.apvts(), up::id::gain_db, "GAIN"), phase(p.apvts(), up::id::phase_rotation_deg, "PHASE ROT"),
      polarityL(p.apvts(), up::id::polarity_l, juce::String::fromUTF8("\xc3\x98 L")),
      polarityR(p.apvts(), up::id::polarity_r, juce::String::fromUTF8("\xc3\x98 R")),
      swapLR(p.apvts(), up::id::swap_lr, "SWAP L/R"), monoSum(p.apvts(), up::id::mono_sum, "MONO"),
      dcBlock(p.apvts(), up::id::dc_block, "DC BLOCK"), muteL(p.apvts(), up::id::mute_l, "MUTE L"),
      muteR(p.apvts(), up::id::mute_r, "MUTE R")
{
    auto& c = content();
    c.addAndMakeVisible(display);
    c.addAndMakeVisible(gain);
    c.addAndMakeVisible(phase);
    for (auto* t : {&polarityL, &polarityR, &swapLR, &monoSum, &dcBlock, &muteL, &muteR})
        c.addAndMakeVisible(*t);
    display.setTheme(theme());
    initialise();
}

void UtilityEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto strip = area.removeFromBottom(110);
    display.setBounds(area.withTrimmedBottom(10));
    gain.setBounds(strip.removeFromLeft(90).withHeight(100));
    strip.removeFromLeft(10);
    phase.setBounds(strip.removeFromLeft(90).withHeight(100));
    strip.removeFromLeft(24);
    auto toggles = strip.withTrimmedTop(30).withHeight(34);
    for (auto* t : {&polarityL, &polarityR, &swapLR, &monoSum, &dcBlock, &muteL, &muteR})
    {
        t->setBounds(toggles.removeFromLeft(86));
        toggles.removeFromLeft(6);
    }
}

void UtilityEditor::themeChanged(const mm::gui::Theme& t) { display.setTheme(t); }

void UtilityEditor::meterFrame(const mm::MeterFrame& f) { display.update(f); }
