#pragma once
#include "gui/Theme.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mm::gui
{
// Collapsible strip for secondary controls (sidechain, oversampling, detector, routing). Open state is the
// editor's to persist (uiState) through onToggle.
class AdvancedDrawer : public juce::Component
{
public:
    explicit AdvancedDrawer(const juce::String& title = "Advanced");
    void setContent(juce::Component& content); // not owned
    void setOpen(bool open, bool notify = false);
    bool isOpen() const noexcept { return open; }
    int preferredHeight() const noexcept { return open ? kHeaderHeight + contentHeight : kHeaderHeight; }
    void setContentHeight(int h) noexcept { contentHeight = h; }
    void setTheme(const Theme& t);
    void resized() override;
    void paint(juce::Graphics&) override;
    std::function<void(bool)> onToggle;
    static constexpr int kHeaderHeight = 22;

private:
    juce::TextButton header;
    juce::Component* content = nullptr;
    Theme theme = Theme::dark(accents::eq);
    bool open = false;
    int contentHeight = 60;
};
} // namespace mm::gui
