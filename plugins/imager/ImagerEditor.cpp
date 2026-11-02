#include "plugins/imager/ImagerEditor.h"
#include "Params.h"
#include "plugins/imager/ImagerProcessor.h"

namespace ip = mm::plugins::imager;

ImagerEditor::ImagerEditor(ImagerProcessor& p)
    : mm::gui::PluginEditorBase(p, mm::gui::Theme::dark(mm::gui::accents::stereo), 920, 580), imager(p),
      spectrum(nullptr, &p.outputAnalyser(), p.getSampleRate() > 0 ? p.getSampleRate() : 48000.0),
      widthLow(p.apvts(), ip::id::width_low_pct, "WIDTH LOW"), xoverLow(p.apvts(), ip::id::xover_low_hz, "CROSSOVER"),
      widthMid(p.apvts(), ip::id::width_mid_pct, "WIDTH MID"), xoverHigh(p.apvts(), ip::id::xover_high_hz, "CROSSOVER"),
      widthHigh(p.apvts(), ip::id::width_high_pct, "WIDTH HIGH"), pan(p.apvts(), ip::id::pan, "PAN"), output(p.apvts(), ip::id::output_db, "OUTPUT"),
      monoBelow(p.apvts(), ip::id::mono_below_hz, "MONO BELOW"), drawer("Mono below"), scopeL(ImagerProcessor::kScopeSize), scopeR(ImagerProcessor::kScopeSize)
{
    auto& c = content();
    c.addAndMakeVisible(gonio);
    c.addAndMakeVisible(spectrum);
    spectrum.setVisiblePrePost(false, true);
    for (auto* k : {&widthLow, &xoverLow, &widthMid, &xoverHigh, &widthHigh, &pan, &output})
        c.addAndMakeVisible(*k);
    drawerContent.addAndMakeVisible(monoBelow);
    drawer.setContent(drawerContent);
    drawer.setContentHeight(84);
    drawer.setOpen(static_cast<bool>(imager.uiState().getProperty("drawerOpen", false)));
    drawer.onToggle = [this](bool open) { imager.uiState().setProperty("drawerOpen", open, nullptr); };
    c.addAndMakeVisible(drawer);
    gonio.setTheme(theme());
    spectrum.setTheme(theme());
    drawer.setTheme(theme());
    initialise();
}

void ImagerEditor::layoutContent(juce::Rectangle<int> area)
{
    area = area.reduced(mm::gui::layout::gutter);
    auto drawerArea = area.removeFromBottom(drawer.preferredHeight());
    drawer.setBounds(drawerArea);
    auto dc = drawerArea.withTrimmedTop(mm::gui::AdvancedDrawer::kHeaderHeight).reduced(0, 2);
    drawerContent.setBounds(dc.withPosition(0, mm::gui::AdvancedDrawer::kHeaderHeight));
    monoBelow.setBounds(drawerContent.getLocalBounds().withWidth(140).reduced(6, 0));
    area.removeFromBottom(6);
    auto strip = area.removeFromBottom(112);
    const int colW = strip.getWidth() / 7;
    for (auto* k : {&widthLow, &xoverLow, &widthMid, &xoverHigh, &widthHigh, &pan, &output})
        k->setBounds(strip.removeFromLeft(colW).reduced(4, 0));
    area.removeFromBottom(8);
    auto right = area.removeFromRight(juce::jmax(220, area.getWidth() - area.getHeight() - 8));
    right.removeFromLeft(8);
    spectrum.setBounds(right.removeFromTop(area.getHeight() / 2));
    gonio.setBounds(area);
}

void ImagerEditor::themeChanged(const mm::gui::Theme& t)
{
    gonio.setTheme(t);
    spectrum.setTheme(t);
    drawer.setTheme(t);
}

void ImagerEditor::meterFrame(const mm::MeterFrame&)
{
    const int n = imager.pullScope(scopeL.data(), scopeR.data(), 2048);
    gonio.pushScope(scopeL.data(), scopeR.data(), n);
    const float corr = imager.currentCorrelation();
    gonio.setCorrelation(corr);
    spectrum.setSampleRate(imager.getSampleRate() > 0 ? imager.getSampleRate() : 48000.0);
    spectrum.pull();
    const float mono = imager.apvts().getRawParameterValue(ip::id::mono_below_hz)->load();
    spectrum.setMarkerHz(mono >= 5.0f ? mono : 0.0f);
    footerBar().setStatusSuffix("correlation " + juce::String(corr, 2) + (mono >= 5.0f ? juce::String::fromUTF8(" \xc2\xb7 mono below ") + juce::String(juce::roundToInt(mono)) + " Hz" : juce::String()));
}
