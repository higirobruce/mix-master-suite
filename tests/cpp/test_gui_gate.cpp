#include "Params.h" // generated for gate
#include "gui/GainReductionMeter.h"
#include "gui/HistoryDisplay.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"
#include "plugins/gate/GateEditor.h"
#include "plugins/gate/GateProcessor.h"
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

TEST_CASE("gate editor: every parameter has a control; history with closed shading, curve and GR meter present")
{
    GuiInit g;
    GateProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    collect(*e, ids);
    const auto& table = mm::plugins::gate::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) != "header")
            REQUIRE(ids.count(spec.id) == 1);
    auto* hist = find<mm::gui::HistoryDisplay>(*e);
    REQUIRE(hist != nullptr);
    REQUIRE(hist->shadesClosed());
    REQUIRE(find<mm::gui::TransferCurveDisplay>(*e) != nullptr);
    REQUIRE(find<mm::gui::GainReductionMeter>(*e) != nullptr);
    REQUIRE(e->getWidth() == 920);
}

TEST_CASE("gate editor: curve follows the mode and the threshold drags on the history")
{
    GuiInit g;
    GateProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    auto* curve = find<mm::gui::TransferCurveDisplay>(*e);
    REQUIRE(curve != nullptr);
    // gate mode, threshold -40, range -80: below threshold the output sits at the range
    REQUIRE(curve->outputDbFor(-50.0f) == Catch::Approx(-130.0f).margin(0.01f));
    REQUIRE(curve->outputDbFor(-30.0f) == Catch::Approx(-30.0f).margin(0.01f));
    auto* mode = p.apvts().getParameter("mode");
    mode->setValueNotifyingHost(1.0f); // expander, ratio 4: -50 -> -40 + 4 * -10 = -80
    REQUIRE(curve->outputDbFor(-50.0f) == Catch::Approx(-80.0f).margin(0.01f));
    auto* hist = find<mm::gui::HistoryDisplay>(*e);
    REQUIRE(hist != nullptr);
    hist->dragThresholdTo(hist->yForDb(-24.0f));
    REQUIRE(p.apvts().getRawParameterValue("threshold_db")->load() == Catch::Approx(-24.0f).margin(0.5f));
}
