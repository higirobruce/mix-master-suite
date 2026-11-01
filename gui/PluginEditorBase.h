#pragma once
#include "gui/Footer.h"
#include "gui/HeaderBar.h"
#include "gui/MMLookAndFeel.h"
#include "gui/Theme.h"
#include "plugin_base/PluginProcessorBase.h"

namespace mm::gui
{
// Editor base: header + content + footer in unscaled "base" coordinates, scaled 50-200 % with a fixed aspect ratio.
// Subclasses add their components to content() and lay them out in base coordinates in layoutContent().
class PluginEditorBase : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    PluginEditorBase(PluginProcessorBase& processor, Theme theme, int baseWidth, int baseHeight);
    ~PluginEditorBase() override;

    // Subclasses call this at the end of their constructor: applies the saved theme/scale and lays out once.
    void initialise();
    void setTheme(Theme theme);
    const Theme& theme() const noexcept { return currentTheme; }
    void setScale(float scale);
    float scale() const noexcept { return currentScale; }
    juce::Component& content() noexcept { return contentArea; }
    juce::Rectangle<int> contentBounds() const noexcept { return contentArea.getLocalBounds(); }
    Footer& footerBar() noexcept { return footer; }

    void resized() override;
    void paint(juce::Graphics&) override;

protected:
    virtual void layoutContent(juce::Rectangle<int> area) = 0; // base coordinates
    virtual void themeChanged(const Theme&) {}
    virtual void meterFrame(const MeterFrame&) {}
    PluginProcessorBase& processor;
    MMLookAndFeel lookAndFeel;

private:
    void timerCallback() override;
    Theme currentTheme;
    const int baseW, baseH;
    float currentScale = 1.0f;
    juce::Component root;
    HeaderBar header;
    juce::Component contentArea;
    Footer footer;
    bool applyingBounds = false;
    bool initialised = false;
};
} // namespace mm::gui
