#include "Params.h" // generated for soft_clipper
#include "gui/GainReductionMeter.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"
#include "plugins/soft_clipper/SoftClipperEditor.h"
#include "plugins/soft_clipper/SoftClipperProcessor.h"
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
void collect(juce::Component& c, std::set<std::string>& ids, int& curves, int& meters)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    if (dynamic_cast<mm::gui::TransferCurveDisplay*>(&c)) ++curves;
    if (dynamic_cast<mm::gui::GainReductionMeter*>(&c)) ++meters;
    for (auto* child : c.getChildren())
        collect(*child, ids, curves, meters);
}
} // namespace

TEST_CASE("soft clipper editor creates, has a control for every parameter, a transfer curve and a clip meter")
{
    GuiInit g;
    SoftClipperProcessor p;
    for (int i = 0; i < 5; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    int curves = 0, meters = 0;
    collect(*e, ids, curves, meters);
    REQUIRE(curves == 1);
    REQUIRE(meters == 1);
    const auto& table = mm::plugins::soft_clipper::table();
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
