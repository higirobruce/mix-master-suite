#pragma once
#include <juce_graphics/juce_graphics.h>

namespace mm::gui
{
// Visual language (spec UI/UX): near-black background, one accent per family, colour carries meaning only.
struct Theme
{
    juce::Colour bg, panel, panelRaised, text, textDim, accent, meterLow, meterMid, meterHigh, grid, outline;
    float cornerRadius = 4.0f;
    bool isDark = true;

    static Theme dark(juce::Colour accent)
    {
        Theme t;
        t.bg = juce::Colour(0xff141518);
        t.panel = juce::Colour(0xff1c1e23);
        t.panelRaised = juce::Colour(0xff23262c);
        t.text = juce::Colour(0xffe8e6e1);
        t.textDim = juce::Colour(0xff9b9ea6);
        t.accent = accent;
        t.meterLow = juce::Colour(0xff5fbf8a);
        t.meterMid = juce::Colour(0xffe0b45c);
        t.meterHigh = juce::Colour(0xffe05c5c);
        t.grid = juce::Colour(0xff2b2e35);
        t.outline = juce::Colour(0xff30343c);
        t.isDark = true;
        return t;
    }
    static Theme light(juce::Colour accent)
    {
        Theme t;
        t.bg = juce::Colour(0xfff4f3ef);
        t.panel = juce::Colour(0xffffffff);
        t.panelRaised = juce::Colour(0xffeceae4);
        t.text = juce::Colour(0xff1d1c19);
        t.textDim = juce::Colour(0xff5f5c55);
        t.accent = accent.darker(0.35f);
        t.meterLow = juce::Colour(0xff2d8a5a);
        t.meterMid = juce::Colour(0xff9a6f12);
        t.meterHigh = juce::Colour(0xffb83232);
        t.grid = juce::Colour(0xffdddad2);
        t.outline = juce::Colour(0xffcfcbc0);
        t.isDark = false;
        return t;
    }
};

namespace accents
{
inline const juce::Colour eq{0xff5b9de8};
inline const juce::Colour dynamics{0xffe0b45c};
inline const juce::Colour timeSpace{0xff3fbfb0};
inline const juce::Colour stereo{0xff9b7be8};
inline const juce::Colour mastering{0xffd8dde4};
inline const juce::Colour utility{0xffd8dde4};
inline juce::Colour forFamily(const juce::String& family)
{
    if (family == "eq") return eq;
    if (family == "dynamics") return dynamics;
    if (family == "time_space") return timeSpace;
    if (family == "stereo") return stereo;
    if (family == "mastering") return mastering;
    return utility;
}
} // namespace accents

// Base layout grammar, in unscaled pixels (the editor scales 50-200 %).
namespace layout
{
inline constexpr int headerHeight = 44;
inline constexpr int footerHeight = 56;
inline constexpr int gutter = 12;
inline constexpr int knobSize = 64;
} // namespace layout
} // namespace mm::gui
