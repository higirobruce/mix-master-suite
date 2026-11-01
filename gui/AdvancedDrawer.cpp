#include "gui/AdvancedDrawer.h"

namespace mm::gui
{
AdvancedDrawer::AdvancedDrawer(const juce::String& title) : header(title)
{
    header.setClickingTogglesState(true);
    header.onClick = [this] { setOpen(header.getToggleState(), true); };
    addAndMakeVisible(header);
}

void AdvancedDrawer::setContent(juce::Component& c)
{
    content = &c;
    addChildComponent(c);
    c.setVisible(open);
    resized();
}

void AdvancedDrawer::setOpen(bool o, bool notify)
{
    open = o;
    header.setToggleState(o, juce::dontSendNotification);
    if (content != nullptr)
        content->setVisible(o);
    if (auto* parent = getParentComponent())
        parent->resized();
    resized();
    if (notify && onToggle)
        onToggle(o);
}

void AdvancedDrawer::setTheme(const Theme& t)
{
    theme = t;
    repaint();
}

void AdvancedDrawer::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(kHeaderHeight).removeFromLeft(110));
    if (content != nullptr)
        content->setBounds(area);
}

void AdvancedDrawer::paint(juce::Graphics& g)
{
    g.setColour(theme.outline);
    g.fillRect(0, kHeaderHeight / 2, getWidth(), 1);
}
} // namespace mm::gui
