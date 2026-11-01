#pragma once
#include "gui/Theme.h"
#include "gui/Toggle.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <functional>

namespace mm::gui
{
// Shared header: plugin name, preset menu, A/B/C/D, undo/redo, delta, gain-matched bypass, settings gear.
class HeaderBar : public juce::Component
{
public:
    HeaderBar(PluginProcessorBase& processor, const juce::String& pluginName);
    void setTheme(const Theme& t);
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh(); // from the editor timer: preset name, slot highlight, delta state

    std::function<void(bool dark)> onThemeChange;
    std::function<void(float scale)> onScaleChange;

private:
    void showPresetMenu();
    void showSettingsMenu();
    PluginProcessorBase& proc;
    Theme theme = Theme::dark(accents::utility);
    juce::Label name;
    juce::TextButton presetButton;
    std::array<juce::TextButton, 4> slots{juce::TextButton("A"), juce::TextButton("B"), juce::TextButton("C"),
                                          juce::TextButton("D")};
    juce::TextButton undoButton{"Undo"}, redoButton{"Redo"}, deltaButton{juce::String::fromUTF8("\xce\x94 Delta")},
        settingsButton{juce::String::fromUTF8("\xe2\x9a\x99")};
    Toggle bypass;
};
} // namespace mm::gui
