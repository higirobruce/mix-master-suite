#include "plugins/lp_eq/LpEqEditor.h"
#include "Params.h"
#include "plugins/lp_eq/LpEqProcessor.h"

namespace lp = mm::plugins::lp_eq;

namespace
{
const char* kIds[8][7] = {
    {lp::id::b1_enabled, lp::id::b1_type, lp::id::b1_freq_hz, lp::id::b1_gain_db, lp::id::b1_q, lp::id::b1_phase, lp::id::b1_ms},
    {lp::id::b2_enabled, lp::id::b2_type, lp::id::b2_freq_hz, lp::id::b2_gain_db, lp::id::b2_q, lp::id::b2_phase, lp::id::b2_ms},
    {lp::id::b3_enabled, lp::id::b3_type, lp::id::b3_freq_hz, lp::id::b3_gain_db, lp::id::b3_q, lp::id::b3_phase, lp::id::b3_ms},
    {lp::id::b4_enabled, lp::id::b4_type, lp::id::b4_freq_hz, lp::id::b4_gain_db, lp::id::b4_q, lp::id::b4_phase, lp::id::b4_ms},
    {lp::id::b5_enabled, lp::id::b5_type, lp::id::b5_freq_hz, lp::id::b5_gain_db, lp::id::b5_q, lp::id::b5_phase, lp::id::b5_ms},
    {lp::id::b6_enabled, lp::id::b6_type, lp::id::b6_freq_hz, lp::id::b6_gain_db, lp::id::b6_q, lp::id::b6_phase, lp::id::b6_ms},
    {lp::id::b7_enabled, lp::id::b7_type, lp::id::b7_freq_hz, lp::id::b7_gain_db, lp::id::b7_q, lp::id::b7_phase, lp::id::b7_ms},
    {lp::id::b8_enabled, lp::id::b8_type, lp::id::b8_freq_hz, lp::id::b8_gain_db, lp::id::b8_q, lp::id::b8_phase, lp::id::b8_ms},
};
const juce::Colour kBandColours[8] = {juce::Colour(0xff9aa5b1), juce::Colour(0xff6fb3ff), juce::Colour(0xff5bd0c8), juce::Colour(0xffe0b45c),
                                      juce::Colour(0xffe08a5c), juce::Colour(0xffb08ae8), juce::Colour(0xffd8dde4), juce::Colour(0xff9aa5b1)};

void setReal(juce::AudioProcessorValueTreeState& apvts, const char* id, double value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        const auto& r = p->getNormalisableRange();
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(r.start, r.end, static_cast<float>(value))));
    }
}
} // namespace

LpEqEditor::LpEqEditor(LpEqProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::mastering), 920, 580), eq(p),
      spectrum(&p.preAnalyser(), &p.postAnalyser(), p.getSampleRate() > 0 ? p.getSampleRate() : 48000.0),
      curve([this] {
                std::vector<mm::gui::CurveDisplay::Node> nodes;
                for (int i = 0; i < LpEqProcessor::kBands; ++i)
                {
                    const auto b = eq.band(i);
                    nodes.push_back({i, b.enabled, b.freq, b.gainDb, b.q, kBandColours[i]});
                }
                return nodes;
            },
            [this](double hz) { return eq.staticResponseAt(hz); }),
      drawer("Advanced"), firLength(p.apvts(), lp::id::fir_length, "FIR LENGTH")
{
    auto& c = content();
    c.addAndMakeVisible(spectrum);
    c.addAndMakeVisible(curve);
    curve.onGesture = [this](int band, bool starting) {
        auto* par = eq.apvts().getParameter(kIds[band][2]);
        if (starting) par->beginChangeGesture(); else par->endChangeGesture();
    };
    curve.onNodeMove = [this](int band, double freq, double gainDb) {
        setReal(eq.apvts(), kIds[band][2], freq);
        setReal(eq.apvts(), kIds[band][3], gainDb);
    };
    curve.onNodeQ = [this](int band, double factor) { setReal(eq.apvts(), kIds[band][4], eq.band(band).q * factor); };
    for (int i = 0; i < LpEqProcessor::kBands; ++i)
    {
        auto& b = bands[static_cast<size_t>(i)];
        b.enabled = std::make_unique<mm::gui::Toggle>(p.apvts(), kIds[i][0], juce::String(i + 1));
        b.type = std::make_unique<mm::gui::Selector>(p.apvts(), kIds[i][1], "TYPE");
        b.freq = std::make_unique<mm::gui::Knob>(p.apvts(), kIds[i][2], "FREQ");
        b.gain = std::make_unique<mm::gui::Knob>(p.apvts(), kIds[i][3], "GAIN");
        b.q = std::make_unique<mm::gui::Knob>(p.apvts(), kIds[i][4], "Q");
        b.phase = std::make_unique<mm::gui::Selector>(p.apvts(), kIds[i][5], "PHASE");
        b.ms = std::make_unique<mm::gui::Selector>(p.apvts(), kIds[i][6], "CH");
        for (juce::Component* comp : {static_cast<juce::Component*>(b.enabled.get()), static_cast<juce::Component*>(b.type.get()),
                                      static_cast<juce::Component*>(b.freq.get()), static_cast<juce::Component*>(b.gain.get()),
                                      static_cast<juce::Component*>(b.q.get()), static_cast<juce::Component*>(b.phase.get()),
                                      static_cast<juce::Component*>(b.ms.get())})
            c.addAndMakeVisible(comp);
    }
    drawerContent.addAndMakeVisible(firLength);
    drawerContent.addAndMakeVisible(latencyLabel);
    latencyLabel.setJustificationType(juce::Justification::centredLeft);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(44);
    drawer.setOpen(static_cast<bool>(eq.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { eq.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    spectrum.setMarkerPrefix(juce::String::fromUTF8("\xc2\xb1" "0.1 dB above "));
    spectrum.setTheme(theme());
    curve.setTheme(theme());
    drawer.setTheme(theme());
    updateLatencyLabel();
    initialise();
}

juce::String LpEqEditor::latencyText() const
{
    const double rate = eq.getSampleRate() > 0 ? eq.getSampleRate() : 48000.0;
    const int latency = eq.getLatencySamples() > 0 ? eq.getLatencySamples() : LpEqProcessor::kMinPartition + (eq.firLength() - 2) / 2;
    return juce::String(1000.0 * latency / rate, 1) + " ms latency \xc2\xb7 floor " + juce::String(eq.resolutionHz(), 0) + " Hz";
}

void LpEqEditor::updateLatencyLabel()
{
    latencyLabel.setText(latencyText(), juce::dontSendNotification);
    footerBar().setStatusSuffix("floor " + juce::String(eq.resolutionHz(), 0) + " Hz"); // the footer shows the latency itself
}

void LpEqEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter, 4);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    auto dc = drawerArea.withZeroOrigin();
    drawerContent.setBounds(dc.withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    auto row = drawerContent.getLocalBounds().withHeight(44).reduced(8, 4);
    firLength.setBounds(row.removeFromLeft(260).reduced(4, 0));
    latencyLabel.setBounds(row.reduced(8, 0));
    area.removeFromBottom(4);
    auto strip = area.removeFromBottom(232);
    const int colW = strip.getWidth() / LpEqProcessor::kBands;
    for (auto& b : bands)
    {
        auto col = strip.removeFromLeft(colW).reduced(3, 0);
        b.enabled->setBounds(col.removeFromTop(22));
        col.removeFromTop(2);
        b.type->setBounds(col.removeFromTop(30));
        auto knobs = col.removeFromTop(74);
        const int kw = knobs.getWidth() / 3;
        b.freq->setBounds(knobs.removeFromLeft(kw));
        b.gain->setBounds(knobs.removeFromLeft(kw));
        b.q->setBounds(knobs);
        col.removeFromTop(2);
        b.phase->setBounds(col.removeFromTop(30));
        col.removeFromTop(2);
        b.ms->setBounds(col.removeFromTop(30));
    }
    area.removeFromBottom(6);
    spectrum.setBounds(area);
    curve.setBounds(area);
}

void LpEqEditor::themeChanged(const mm::gui::Theme& t)
{
    spectrum.setTheme(t);
    curve.setTheme(t);
    drawer.setTheme(t);
    latencyLabel.setColour(juce::Label::textColourId, t.textDim);
}

void LpEqEditor::meterFrame(const mm::MeterFrame&)
{
    spectrum.setSampleRate(eq.getSampleRate() > 0 ? eq.getSampleRate() : 48000.0);
    spectrum.pull();
    spectrum.setMarkerHz(static_cast<float>(2.0 * eq.resolutionHz())); // where the ±0.1 dB guarantee starts
    curve.refreshFromProcessor();
    updateLatencyLabel();
}
