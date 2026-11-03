#include "Params.h" // generated for multiband_comp
#include "gui/Knob.h"
#include "gui/MultibandDisplay.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "plugins/multiband_comp/MultibandCompEditor.h"
#include "plugins/multiband_comp/MultibandCompProcessor.h"
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
void collect(juce::Component& c, std::set<std::string>& ids, int& displays)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    if (dynamic_cast<mm::gui::MultibandDisplay*>(&c)) ++displays;
    for (auto* child : c.getChildren())
        collect(*child, ids, displays);
}
mm::gui::MultibandDisplay* findDisplay(juce::Component& c)
{
    if (auto* d = dynamic_cast<mm::gui::MultibandDisplay*>(&c)) return d;
    for (auto* child : c.getChildren())
        if (auto* d = findDisplay(*child)) return d;
    return nullptr;
}
} // namespace

TEST_CASE("multiband editor creates, has a control for every parameter, a band display with draggable crossovers and band focus")
{
    GuiInit g;
    MultibandCompProcessor p;
    for (int i = 0; i < 5; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    int displays = 0;
    collect(*e, ids, displays);
    REQUIRE(displays == 1);
    const auto& table = mm::plugins::multiband_comp::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
    {
        const std::string group(spec.group);
        if (group == "band" || group == "xover" || group == "main" || group == "advanced" || group == "footer")
            REQUIRE(ids.count(spec.id) == 1);
    }
    auto* display = findDisplay(*e);
    REQUIRE(display != nullptr);
    REQUIRE(display->numCrossovers() == 3); // four bands
    // dragging crossover 2 to the 1 kHz pixel moves the parameter
    display->dragCrossoverTo(1, display->xForHz(1000.0));
    REQUIRE(p.apvts().getRawParameterValue("xover_2_hz")->load() == Catch::Approx(1000.0f).margin(20.0f));
    // clicking a band focuses it
    display->focusBand(2);
    REQUIRE(display->focusedBand() == 2);
    auto* editor = dynamic_cast<MultibandCompEditor*>(e.get());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->focusedBand() == 2);
    REQUIRE(e->getWidth() == 920);
    REQUIRE(e->getHeight() == 580);
}
