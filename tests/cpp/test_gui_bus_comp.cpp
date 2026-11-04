#include "Params.h" // generated for bus_comp
#include "gui/Knob.h"
#include "gui/NeedleMeter.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"
#include "plugins/bus_comp/BusCompEditor.h"
#include "plugins/bus_comp/BusCompProcessor.h"
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
void collect(juce::Component& c, std::set<std::string>& ids, int& needles, int& curves)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    if (dynamic_cast<mm::gui::NeedleMeter*>(&c)) ++needles;
    if (dynamic_cast<mm::gui::TransferCurveDisplay*>(&c)) ++curves;
    for (auto* child : c.getChildren())
        collect(*child, ids, needles, curves);
}
} // namespace

TEST_CASE("bus compressor editor creates, has a control for every parameter, a needle meter and a transfer curve")
{
    GuiInit g;
    BusCompProcessor p;
    for (int i = 0; i < 5; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    int needles = 0, curves = 0;
    collect(*e, ids, needles, curves);
    REQUIRE(needles == 1);
    REQUIRE(curves == 1);
    const auto& table = mm::plugins::bus_comp::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
    {
        const std::string group(spec.group);
        if (group == "main" || group == "advanced" || group == "footer")
            REQUIRE(ids.count(spec.id) == 1);
    }
    REQUIRE(e->getWidth() == 920);
    REQUIRE(e->getHeight() == 580);
}
