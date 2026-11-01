#include "plugins/channel_eq/ChannelEqEditor.h"
#include "Params.h"
#include "plugins/channel_eq/ChannelEqProcessor.h"

namespace ep = mm::plugins::channel_eq;

namespace
{
const char* kBandIds[5][5] = {
    {ep::id::b1_enabled, ep::id::b1_type, ep::id::b1_freq_hz, ep::id::b1_gain_db, ep::id::b1_q},
    {ep::id::b2_enabled, ep::id::b2_type, ep::id::b2_freq_hz, ep::id::b2_gain_db, ep::id::b2_q},
    {ep::id::b3_enabled, ep::id::b3_type, ep::id::b3_freq_hz, ep::id::b3_gain_db, ep::id::b3_q},
    {ep::id::b4_enabled, ep::id::b4_type, ep::id::b4_freq_hz, ep::id::b4_gain_db, ep::id::b4_q},
    {ep::id::b5_enabled, ep::id::b5_type, ep::id::b5_freq_hz, ep::id::b5_gain_db, ep::id::b5_q},
};
const juce::Colour kBandColours[5] = {juce::Colour(0xff6fb3ff), juce::Colour(0xff5bd0c8), juce::Colour(0xffe0b45c),
                                      juce::Colour(0xffe08a5c), juce::Colour(0xffb08ae8)};

void setReal(juce::AudioProcessorValueTreeState& apvts, const char* id, double value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        const auto& r = p->getNormalisableRange();
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(r.start, r.end, static_cast<float>(value))));
    }
}
} // namespace

ChannelEqEditor::ChannelEqEditor(ChannelEqProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::eq), 920, 580), eq(p),
      spectrum(&p.preAnalyser(), &p.postAnalyser(), p.getSampleRate() > 0 ? p.getSampleRate() : 48000.0),
      curve([this] {
                std::vector<mm::gui::CurveDisplay::Node> nodes;
                for (int i = 0; i < 5; ++i)
                {
                    const auto b = eq.band(i);
                    nodes.push_back({i, b.enabled, b.freq, b.gainDb, b.q, kBandColours[i]});
                }
                return nodes;
            },
            [this](double hz) { return eq.evaluateAt(hz); }),
      drawer("Advanced"), qMode(p.apvts(), ep::id::q_mode, "Q MODE"), oversample(p.apvts(), ep::id::oversample, "OVERSAMPLING"),
      analyser(p.apvts(), ep::id::analyser, "ANALYSER")
{
    auto& c = content();
    c.addAndMakeVisible(spectrum);
    c.addAndMakeVisible(curve);
    curve.onNodeMove = [this](int band, double freq, double gainDb) {
        setReal(eq.apvts(), kBandIds[band][2], freq);
        setReal(eq.apvts(), kBandIds[band][3], gainDb);
        eq.updateDesignForTests();
    };
    curve.onNodeQ = [this](int band, double factor) {
        const float q = eq.apvts().getRawParameterValue(kBandIds[band][4])->load();
        setReal(eq.apvts(), kBandIds[band][4], q * factor);
        eq.updateDesignForTests();
    };
    curve.onNodeSolo = [this](int band, bool solo) {
        if (solo)
        {
            soloBand = band;
            for (int i = 0; i < 5; ++i)
            {
                soloSaved[static_cast<size_t>(i)] = eq.apvts().getRawParameterValue(kBandIds[i][0])->load() >= 0.5f;
                if (i != band)
                    setReal(eq.apvts(), kBandIds[i][0], 0.0);
            }
        }
        else if (soloBand >= 0)
        {
            for (int i = 0; i < 5; ++i)
                setReal(eq.apvts(), kBandIds[i][0], soloSaved[static_cast<size_t>(i)] ? 1.0 : 0.0);
            soloBand = -1;
        }
    };

    for (int i = 0; i < 5; ++i)
    {
        auto& col = bands[static_cast<size_t>(i)];
        col.enabled = std::make_unique<mm::gui::Toggle>(p.apvts(), kBandIds[i][0], juce::String(i + 1));
        col.type = std::make_unique<mm::gui::Selector>(p.apvts(), kBandIds[i][1], "TYPE");
        col.freq = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[i][2], "FREQ");
        col.gain = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[i][3], "GAIN");
        col.q = std::make_unique<mm::gui::Knob>(p.apvts(), kBandIds[i][4], "Q");
        col.enabled->button().setColour(juce::TextButton::buttonOnColourId, kBandColours[i]);
        c.addAndMakeVisible(*col.enabled);
        c.addAndMakeVisible(*col.type);
        c.addAndMakeVisible(*col.freq);
        c.addAndMakeVisible(*col.gain);
        c.addAndMakeVisible(*col.q);
    }
    hp.enabled = std::make_unique<mm::gui::Toggle>(p.apvts(), ep::id::hp_enabled, "HP");
    hp.freq = std::make_unique<mm::gui::Knob>(p.apvts(), ep::id::hp_freq_hz, "FREQ");
    hp.slope = std::make_unique<mm::gui::Selector>(p.apvts(), ep::id::hp_slope, "SLOPE");
    lp.enabled = std::make_unique<mm::gui::Toggle>(p.apvts(), ep::id::lp_enabled, "LP");
    lp.freq = std::make_unique<mm::gui::Knob>(p.apvts(), ep::id::lp_freq_hz, "FREQ");
    lp.slope = std::make_unique<mm::gui::Selector>(p.apvts(), ep::id::lp_slope, "SLOPE");
    for (auto* f : {&hp, &lp})
    {
        c.addAndMakeVisible(*f->enabled);
        c.addAndMakeVisible(*f->freq);
        c.addAndMakeVisible(*f->slope);
    }

    drawerContent.addAndMakeVisible(qMode);
    drawerContent.addAndMakeVisible(oversample);
    drawerContent.addAndMakeVisible(analyser);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(44);
    drawer.setOpen(static_cast<bool>(eq.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { eq.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);

    spectrum.setTheme(theme());
    curve.setTheme(theme());
    drawer.setTheme(theme());
    initialise();
}

void ChannelEqEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    auto dc = drawerArea.withTrimmedTop(mm::gui::AdvancedDrawer::kHeaderHeight).reduced(0, 2);
    drawerContent.setBounds(dc.withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    auto row = drawerContent.getLocalBounds();
    const int third = row.getWidth() / 3;
    qMode.setBounds(row.removeFromLeft(third).reduced(4, 0));
    oversample.setBounds(row.removeFromLeft(third).reduced(4, 0));
    analyser.setBounds(row.reduced(4, 0));
    area.removeFromBottom(6);

    auto strip = area.removeFromBottom(176);
    const int colW = strip.getWidth() / 7;
    auto layoutFilter = [&](FilterColumn& f, juce::Rectangle<int> col) {
        col = col.reduced(6, 0);
        f.enabled->setBounds(col.removeFromTop(22));
        col.removeFromTop(2);
        f.slope->setBounds(col.removeFromTop(34));
        f.freq->setBounds(col.removeFromTop(78));
    };
    layoutFilter(hp, strip.removeFromLeft(colW));
    for (auto& b : bands)
    {
        auto col = strip.removeFromLeft(colW).reduced(6, 0);
        b.enabled->setBounds(col.removeFromTop(22));
        col.removeFromTop(2);
        b.type->setBounds(col.removeFromTop(34));
        auto knobs = col.removeFromTop(78);
        const int kw = knobs.getWidth() / 3;
        b.freq->setBounds(knobs.removeFromLeft(kw));
        b.gain->setBounds(knobs.removeFromLeft(kw));
        b.q->setBounds(knobs);
    }
    layoutFilter(lp, strip);
    area.removeFromBottom(8);
    spectrum.setBounds(area);
    curve.setBounds(area);
}

void ChannelEqEditor::themeChanged(const mm::gui::Theme& t)
{
    spectrum.setTheme(t);
    curve.setTheme(t);
    drawer.setTheme(t);
}

void ChannelEqEditor::meterFrame(const mm::MeterFrame&)
{
    spectrum.setSampleRate(eq.getSampleRate() > 0 ? eq.getSampleRate() : 48000.0);
    const int mode = static_cast<int>(eq.apvts().getRawParameterValue(ep::id::analyser)->load());
    spectrum.setVisiblePrePost(mode == 1 || mode == 3, mode == 2 || mode == 3);
    spectrum.pull();
    curve.refreshFromProcessor();
    footerBar().setStatusSuffix(eq.apvts().getRawParameterValue(ep::id::oversample)->load() >= 0.5f ? juce::String::fromUTF8("2\xc3\x97 OS on")
                                                                                                    : juce::String::fromUTF8("2\xc3\x97 OS off"));
}
