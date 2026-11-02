#include "Params.h" // generated for delay
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/TapDisplay.h"
#include "gui/Toggle.h"
#include "plugins/delay/DelayEditor.h"
#include "plugins/delay/DelayProcessor.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

namespace
{
struct GuiInit
{
    juce::ScopedJuceInitialiser_GUI init;
};
template <typename T> T* find(juce::Component& c)
{
    if (auto* d = dynamic_cast<T*>(&c)) return d;
    for (auto* child : c.getChildren())
        if (auto* d = find<T>(*child)) return d;
    return nullptr;
}
void collect(juce::Component& c, std::set<std::string>& ids)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    for (auto* child : c.getChildren())
        collect(*child, ids);
}
} // namespace

TEST_CASE("delay editor: every parameter has a control and a tap display that snaps to the tempo grid")
{
    GuiInit g;
    DelayProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    collect(*e, ids);
    const auto& table = mm::plugins::delay::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) != "header")
            REQUIRE(ids.count(spec.id) == 1);
    auto* taps = find<mm::gui::TapDisplay>(*e);
    REQUIRE(taps != nullptr);
    REQUIRE(e->getWidth() == 920);
    // drag the left tap to 253 ms at 120 BPM: within 5 % of an eighth note (250 ms) -> snaps and labels "1/8"
    taps->setBpm(120.0);
    taps->dragTap(0, taps->xForMs(253.0));
    REQUIRE(p.apvts().getRawParameterValue("time_l_ms")->load() == Catch::Approx(250.0f).margin(1.0f));
    REQUIRE(taps->snappedLabel() == "1/8");
    // far from any note value: no snap, no label
    taps->dragTap(0, taps->xForMs(1234.0));
    REQUIRE(p.apvts().getRawParameterValue("time_l_ms")->load() == Catch::Approx(1234.0f).margin(1234.0f * 0.02f));
    REQUIRE(taps->snappedLabel().isEmpty());
}
