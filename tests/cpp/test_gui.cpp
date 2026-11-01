#include "Params.h" // generated for utility
#include "gui/Knob.h"
#include "gui/Theme.h"
#include "gui/Toggle.h"
#include "plugins/utility/UtilityEditor.h"
#include "plugins/utility/UtilityProcessor.h"
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

namespace
{
struct GuiInit
{
    juce::ScopedJuceInitialiser_GUI init;
};

void collectParamIds(juce::Component& c, std::set<std::string>& ids)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c))
        ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c))
        ids.insert(t->paramId());
    for (auto* child : c.getChildren())
        collectParamIds(*child, ids);
}
} // namespace

TEST_CASE("editor creates and destroys without leaks")
{
    GuiInit g;
    UtilityProcessor p;
    for (int i = 0; i < 50; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
}

TEST_CASE("every control is attached to a declared parameter and every main parameter has a control")
{
    GuiInit g;
    UtilityProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    collectParamIds(*e, ids);
    REQUIRE_FALSE(ids.empty());
    const auto& table = mm::plugins::utility::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) == "main" || std::string(spec.group) == "footer")
            REQUIRE(ids.count(spec.id) == 1);
}

TEST_CASE("resize limits 50-200 percent with fixed aspect")
{
    GuiInit g;
    UtilityProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    REQUIRE(e->getWidth() == 920);
    REQUIRE(e->getHeight() == 580);
    e->setSize(460, 290);
    REQUIRE(e->getWidth() == 460);
    REQUIRE(e->getHeight() == 290);
    e->setSize(1840, 1160);
    REQUIRE(e->getWidth() == 1840);
    REQUIRE(e->getHeight() == 1160);
    e->setSize(100, 100);
    REQUIRE(e->getWidth() == 460);
    REQUIRE(e->getHeight() == 290);
}

TEST_CASE("theme switch repaints")
{
    GuiInit g;
    UtilityProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    auto* base = dynamic_cast<mm::gui::PluginEditorBase*>(e.get());
    REQUIRE(base != nullptr);
    base->setTheme(mm::gui::Theme::light(mm::gui::accents::utility));
    REQUIRE(base->theme().bg == mm::gui::Theme::light(mm::gui::accents::utility).bg);
    base->setTheme(mm::gui::Theme::dark(mm::gui::accents::utility));
    REQUIRE(base->theme().bg == mm::gui::Theme::dark(mm::gui::accents::utility).bg);
}
