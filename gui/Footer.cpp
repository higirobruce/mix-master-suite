#include "gui/Footer.h"

namespace mm::gui
{
Footer::Footer(PluginProcessorBase& p) : trim(p.apvts(), "trim_db", "TRIM")
{
    for (auto* l : {&inLabel, &outLabel})
    {
        l->setFont(juce::FontOptions(10.0f, juce::Font::bold));
        l->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*l);
    }
    for (auto* v : {&inValue, &outValue, &status})
    {
        v->setFont(juce::FontOptions(11.0f));
        v->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*v);
    }
    for (auto* m : {&inL, &inR, &outL, &outR})
        addAndMakeVisible(*m);
    addAndMakeVisible(trim);
    status.setText(juce::String::fromUTF8("Latency 0 ms"), juce::dontSendNotification);
}

void Footer::setTheme(const Theme& t)
{
    theme = t;
    for (auto* m : {&inL, &inR, &outL, &outR})
        m->setTheme(t);
    for (auto* l : {&inLabel, &outLabel, &status})
        l->setColour(juce::Label::textColourId, t.textDim);
    repaint();
}

void Footer::update(const MeterFrame& f)
{
    inL.setLevels(f.peakInDb[0], f.rmsInDb[0]);
    inR.setLevels(f.peakInDb[1], f.rmsInDb[1]);
    outL.setLevels(f.peakOutDb[0], f.rmsOutDb[0]);
    outR.setLevels(f.peakOutDb[1], f.rmsOutDb[1]);
    inValue.setText(juce::String(juce::jmax(f.peakInDb[0], f.peakInDb[1]), 1) + " dB", juce::dontSendNotification);
    outValue.setText(juce::String(juce::jmax(f.peakOutDb[0], f.peakOutDb[1]), 1) + " dB", juce::dontSendNotification);
    status.setText("Latency " + juce::String(f.latencyMs, 1) + " ms", juce::dontSendNotification);
}

void Footer::paint(juce::Graphics& g)
{
    g.setColour(theme.panel);
    g.fillRect(getLocalBounds());
    g.setColour(theme.outline);
    g.fillRect(0, 0, getWidth(), 1);
}

void Footer::resized()
{
    auto area = getLocalBounds().reduced(layout::gutter, 6);
    auto meters = area.removeFromLeft(300);
    auto inArea = meters.removeFromLeft(140);
    inLabel.setBounds(inArea.removeFromLeft(26));
    inL.setBounds(inArea.removeFromLeft(8));
    inArea.removeFromLeft(2);
    inR.setBounds(inArea.removeFromLeft(8));
    inArea.removeFromLeft(6);
    inValue.setBounds(inArea);
    meters.removeFromLeft(10);
    outLabel.setBounds(meters.removeFromLeft(30));
    outL.setBounds(meters.removeFromLeft(8));
    meters.removeFromLeft(2);
    outR.setBounds(meters.removeFromLeft(8));
    meters.removeFromLeft(6);
    outValue.setBounds(meters);
    trim.setBounds(area.removeFromRight(70).withTrimmedTop(-4));
    status.setBounds(area.removeFromRight(240));
}
} // namespace mm::gui
