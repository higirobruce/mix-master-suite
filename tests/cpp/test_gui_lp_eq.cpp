#include "Params.h" // generated for lp_eq
#include "gui/CurveDisplay.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "plugins/lp_eq/LpEqEditor.h"
#include "plugins/lp_eq/LpEqProcessor.h"
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
void collect(juce::Component& c, std::set<std::string>& ids, int& curves)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    if (dynamic_cast<mm::gui::CurveDisplay*>(&c)) ++curves;
    for (auto* child : c.getChildren())
        collect(*child, ids, curves);
}
} // namespace

TEST_CASE("linear-phase eq editor creates, has a control for every band / advanced parameter, a curve and a latency readout")
{
    GuiInit g;
    LpEqProcessor p;
    for (int i = 0; i < 5; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    int curves = 0;
    collect(*e, ids, curves);
    REQUIRE(curves == 1);
    const auto& table = mm::plugins::lp_eq::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
    {
        const std::string group(spec.group);
        if (group == "band" || group == "advanced" || group == "footer")
            REQUIRE(ids.count(spec.id) == 1);
    }
    auto* editor = dynamic_cast<LpEqEditor*>(e.get());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->latencyText().contains("ms")); // the footer shows the FIR's latency cost
    REQUIRE(e->getWidth() == 920);
    REQUIRE(e->getHeight() == 580);
}
