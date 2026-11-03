#include "Params.h" // generated for limiter
#include "gui/CeilingHistory.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "plugins/limiter/LimiterEditor.h"
#include "plugins/limiter/LimiterProcessor.h"
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
void collect(juce::Component& c, std::set<std::string>& ids, int& histories)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    if (dynamic_cast<mm::gui::CeilingHistory*>(&c)) ++histories;
    for (auto* child : c.getChildren())
        collect(*child, ids, histories);
}
} // namespace

TEST_CASE("limiter editor creates, has a control for every parameter, the ceiling history and the loudness readouts")
{
    GuiInit g;
    LimiterProcessor p;
    for (int i = 0; i < 5; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    int histories = 0;
    collect(*e, ids, histories);
    REQUIRE(histories == 1);
    const auto& table = mm::plugins::limiter::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
    {
        const std::string group(spec.group);
        if (group == "main" || group == "advanced" || group == "footer")
            REQUIRE(ids.count(spec.id) == 1);
    }
    auto* editor = dynamic_cast<LimiterEditor*>(e.get());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->readoutText().contains("LUFS"));
    REQUIRE(editor->readoutText().contains("dBTP"));
    REQUIRE(e->getWidth() == 920);
    REQUIRE(e->getHeight() == 580);
}
