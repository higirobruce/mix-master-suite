#include "plugins/saturation/SaturationEditor.h"
#include "Params.h"
#include "plugins/saturation/SaturationProcessor.h"

namespace sp = mm::plugins::saturation;

namespace
{
const char* const kDescriptions[4] = {"Tape: soft, warm, head bump and gentle HF compression",
                                      "Tube: even-order glow that grows with bias",
                                      "Transistor: hard odd-order edge, bias skews it",
                                      "Clip: flat-top limiting colour, soft knee"};
const char* const kWatched[] = {sp::id::drive_db, sp::id::character, sp::id::bias};
} // namespace

SaturationEditor::SaturationEditor(SaturationProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::colour), 920, 580), sat(p),
      drive(p.apvts(), sp::id::drive_db, "DRIVE"), bias(p.apvts(), sp::id::bias, "BIAS"), hfRolloff(p.apvts(), sp::id::hf_rolloff_hz, "HF ROLL-OFF"),
      mix(p.apvts(), sp::id::mix_pct, "MIX"), output(p.apvts(), sp::id::output_db, "OUTPUT"), character(p.apvts(), sp::id::character, "CHARACTER"),
      harmonics(SaturationProcessor::kHarmonics)
{
    auto& c = content();
    for (auto* k : {&drive, &bias, &hfRolloff, &mix, &output})
        c.addAndMakeVisible(*k);
    c.addAndMakeVisible(character);
    c.addAndMakeVisible(description);
    c.addAndMakeVisible(title);
    c.addAndMakeVisible(harmonics);
    title.setText("HARMONICS of a 1 kHz tone at -12 dBFS", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(11.0f)));
    description.setFont(juce::Font(juce::FontOptions(13.0f)));
    description.setJustificationType(juce::Justification::centredLeft);
    for (auto* id : kWatched)
        sat.apvts().addParameterListener(id, this);
    harmonics.setTheme(theme());
    refreshHarmonics();
    initialise();
}

SaturationEditor::~SaturationEditor()
{
    for (auto* id : kWatched)
        sat.apvts().removeParameterListener(id, this);
}

void SaturationEditor::parameterChanged(const juce::String&, float) { dirty.store(true, std::memory_order_relaxed); }

void SaturationEditor::refreshHarmonics()
{
    const int chr = juce::jlimit(0, 3, static_cast<int>(std::lround(sat.apvts().getRawParameterValue(sp::id::character)->load())));
    const float d = sat.apvts().getRawParameterValue(sp::id::drive_db)->load();
    const float b = sat.apvts().getRawParameterValue(sp::id::bias)->load();
    const auto levels = SaturationProcessor::harmonicLevelsDb(d, chr, b); // static stage on the message thread
    harmonics.setLevels(levels.data(), SaturationProcessor::kHarmonics);
    description.setText(kDescriptions[chr], juce::dontSendNotification);
    dirty.store(false, std::memory_order_relaxed);
}

void SaturationEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto strip = area.removeFromBottom(120);
    const int colW = strip.getWidth() / 4;
    for (auto* k : {&bias, &hfRolloff, &mix})
        k->setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    output.setBounds(strip.reduced(4, 0));
    area.removeFromBottom(8);
    auto left = area.removeFromLeft(260);
    character.setBounds(left.removeFromTop(44).reduced(6, 0));
    description.setBounds(left.removeFromTop(40).reduced(6, 0));
    drive.setBounds(left.reduced(10, 4)); // the one large knob (>= 160 px)
    area.removeFromLeft(8);
    title.setBounds(area.removeFromTop(16));
    harmonics.setBounds(area);
}

void SaturationEditor::themeChanged(const mm::gui::Theme& t)
{
    harmonics.setTheme(t);
    description.setColour(juce::Label::textColourId, t.text);
    title.setColour(juce::Label::textColourId, t.textDim);
}

void SaturationEditor::meterFrame(const mm::MeterFrame&)
{
    if (dirty.load(std::memory_order_relaxed))
        refreshHarmonics();
    footerBar().setStatusSuffix(juce::String::fromUTF8("4\xc3\x97 OS \xc2\xb7 ") + juce::String(sat.oversamplerLatency()) + " smp");
}
