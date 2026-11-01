#include "gui/HeaderBar.h"

namespace mm::gui
{
HeaderBar::HeaderBar(PluginProcessorBase& p, const juce::String& pluginName)
    : proc(p), bypass(p.apvts(), "bypass", juce::String::fromUTF8("Bypass \xc2\xb7 gain match"))
{
    name.setText(pluginName.toUpperCase(), juce::dontSendNotification);
    name.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(name);

    presetButton.setButtonText(proc.presets().currentName());
    presetButton.onClick = [this] { showPresetMenu(); };
    presetButton.setTitle("Presets");
    addAndMakeVisible(presetButton);

    for (int i = 0; i < 4; ++i)
    {
        slots[static_cast<size_t>(i)].setRadioGroupId(1001);
        slots[static_cast<size_t>(i)].setClickingTogglesState(false);
        slots[static_cast<size_t>(i)].onClick = [this, i] {
            proc.slots().recall(i);
            refresh();
        };
        addAndMakeVisible(slots[static_cast<size_t>(i)]);
    }
    undoButton.onClick = [this] { proc.undo().undo(); };
    redoButton.onClick = [this] { proc.undo().redo(); };
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);

    deltaButton.setClickingTogglesState(true);
    deltaButton.setTitle("Delta listen");
    deltaButton.onClick = [this] { proc.setDeltaListen(deltaButton.getToggleState()); };
    addAndMakeVisible(deltaButton);
    addAndMakeVisible(bypass);

    settingsButton.setTitle("Settings");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible(settingsButton);
    refresh();
}

void HeaderBar::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

void HeaderBar::refresh()
{
    presetButton.setButtonText(proc.presets().currentName());
    for (int i = 0; i < 4; ++i)
        slots[static_cast<size_t>(i)].setToggleState(proc.slots().active() == i, juce::dontSendNotification);
    deltaButton.setToggleState(proc.deltaListen(), juce::dontSendNotification);
}

void HeaderBar::paint(juce::Graphics& g)
{
    g.setColour(theme.panel);
    g.fillRect(getLocalBounds());
    g.setColour(theme.outline);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
    g.setColour(theme.accent);
    g.fillRect(0, 0, 3, getHeight());
}

void HeaderBar::resized()
{
    auto area = getLocalBounds().reduced(layout::gutter, 8);
    name.setBounds(area.removeFromLeft(150));
    presetButton.setBounds(area.removeFromLeft(180));
    area.removeFromLeft(10);
    for (auto& s : slots)
    {
        s.setBounds(area.removeFromLeft(28));
        area.removeFromLeft(3);
    }
    area.removeFromLeft(8);
    undoButton.setBounds(area.removeFromLeft(48));
    area.removeFromLeft(3);
    redoButton.setBounds(area.removeFromLeft(48));
    settingsButton.setBounds(area.removeFromRight(28));
    area.removeFromRight(6);
    bypass.setBounds(area.removeFromRight(150));
    area.removeFromRight(6);
    deltaButton.setBounds(area.removeFromRight(70));
}

void HeaderBar::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto presets = proc.presets().list();
    int idx = 1;
    for (const auto& info : presets)
        menu.addItem(idx++, (info.factory ? juce::String() : juce::String::fromUTF8("\xe2\x97\x8f ")) + info.name);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetButton), [this, presets](int result) {
        if (result > 0 && result <= static_cast<int>(presets.size()))
        {
            proc.presets().load(presets[static_cast<size_t>(result - 1)]);
            refresh();
        }
    });
}

void HeaderBar::showSettingsMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu themeMenu;
    themeMenu.addItem(1, "Dark", true, theme.isDark);
    themeMenu.addItem(2, "Light", true, !theme.isDark);
    menu.addSubMenu("Theme", themeMenu);
    juce::PopupMenu scale;
    for (int s : {50, 75, 100, 150, 200})
        scale.addItem(100 + s, juce::String(s) + " %");
    menu.addSubMenu("Size", scale);
    menu.addSeparator();
    menu.addItem(999, juce::String::fromUTF8("Offline \xc2\xb7 on this machine"), false);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(settingsButton), [this](int r) {
        if (r == 1 && onThemeChange) onThemeChange(true);
        if (r == 2 && onThemeChange) onThemeChange(false);
        if (r > 100 && r <= 300 && onScaleChange) onScaleChange(static_cast<float>(r - 100) / 100.0f);
    });
}
} // namespace mm::gui
