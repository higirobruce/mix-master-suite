#include "gui/PluginEditorBase.h"

namespace mm::gui
{
PluginEditorBase::PluginEditorBase(PluginProcessorBase& p, Theme t, int baseWidth, int baseHeight)
    : juce::AudioProcessorEditor(p), processor(p), lookAndFeel(t), currentTheme(t), baseW(baseWidth), baseH(baseHeight),
      header(p, p.getName()), footer(p)
{
    setLookAndFeel(&lookAndFeel);
    root.addAndMakeVisible(header);
    root.addAndMakeVisible(contentArea);
    root.addAndMakeVisible(footer);
    addAndMakeVisible(root);
    header.onThemeChange = [this](bool dark) { setTheme(dark ? Theme::dark(currentTheme.accent) : Theme::light(currentTheme.accent)); };
    header.onScaleChange = [this](float s) { setScale(s); };
    header.setTheme(t);
    footer.setTheme(t);

    setResizable(true, true);
    setResizeLimits(baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(baseW) / baseH);

}

void PluginEditorBase::initialise()
{
    float savedScale = static_cast<float>(static_cast<double>(processor.uiState().getProperty("scale", 1.0)));
    savedScale = juce::jlimit(0.5f, 2.0f, savedScale);
    const bool savedDark = static_cast<bool>(processor.uiState().getProperty("dark", true));
    if (!savedDark)
        setTheme(Theme::light(currentTheme.accent));
    initialised = true;
    setSize(juce::roundToInt(static_cast<float>(baseW) * savedScale), juce::roundToInt(static_cast<float>(baseH) * savedScale));
    startTimerHz(60);
}

PluginEditorBase::~PluginEditorBase()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PluginEditorBase::setTheme(Theme t)
{
    currentTheme = t;
    lookAndFeel.setTheme(t);
    header.setTheme(t);
    footer.setTheme(t);
    processor.uiState().setProperty("dark", t.isDark, nullptr);
    themeChanged(t);
    sendLookAndFeelChange();
    repaint();
}

void PluginEditorBase::setScale(float s)
{
    s = juce::jlimit(0.5f, 2.0f, s);
    setSize(juce::roundToInt(static_cast<float>(baseW) * s), juce::roundToInt(static_cast<float>(baseH) * s));
}

void PluginEditorBase::resized()
{
    if (applyingBounds || !initialised)
        return;
    // clamp to 50-200 % and keep the aspect ratio, whatever the host asked for
    const float s = juce::jlimit(0.5f, 2.0f, static_cast<float>(getWidth()) / static_cast<float>(baseW));
    const int w = juce::roundToInt(static_cast<float>(baseW) * s), h = juce::roundToInt(static_cast<float>(baseH) * s);
    if (w != getWidth() || h != getHeight())
    {
        applyingBounds = true;
        setSize(w, h);
        applyingBounds = false;
    }
    currentScale = s;
    processor.uiState().setProperty("scale", static_cast<double>(s), nullptr);
    root.setTransform(juce::AffineTransform::scale(s));
    root.setBounds(0, 0, baseW, baseH);
    auto area = root.getLocalBounds();
    header.setBounds(area.removeFromTop(layout::headerHeight));
    footer.setBounds(area.removeFromBottom(layout::footerHeight));
    contentArea.setBounds(area);
    layoutContent(contentArea.getLocalBounds());
}

void PluginEditorBase::paint(juce::Graphics& g) { g.fillAll(currentTheme.bg); }

void PluginEditorBase::timerCallback()
{
    MeterFrame frame;
    bool got = false;
    while (processor.meterFifo().pop(frame))
        got = true;
    if (got)
    {
        footer.update(frame);
        meterFrame(frame);
    }
    header.refresh();
}
} // namespace mm::gui
