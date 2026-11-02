#include "Params.h" // generated for saturation
#include "gui/HarmonicDisplay.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "plugins/saturation/SaturationEditor.h"
#include "plugins/saturation/SaturationProcessor.h"
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
void collect(juce::Component& c, std::set<std::string>& ids, std::vector<mm::gui::Knob*>& knobs)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) { ids.insert(k->paramId()); knobs.push_back(k); }
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    for (auto* child : c.getChildren())
        collect(*child, ids, knobs);
}
} // namespace

TEST_CASE("saturation editor: every parameter has a control, a large drive knob, a harmonic display and character descriptions")
{
    GuiInit g;
    SaturationProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    std::vector<mm::gui::Knob*> knobs;
    collect(*e, ids, knobs);
    const auto& table = mm::plugins::saturation::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) != "header")
            REQUIRE(ids.count(spec.id) == 1);
    bool bigDrive = false;
    for (auto* k : knobs)
        if (k->paramId() == "drive_db" && k->getWidth() >= 160 && k->getHeight() >= 160)
            bigDrive = true;
    REQUIRE(bigDrive);
    auto* harm = find<mm::gui::HarmonicDisplay>(*e);
    REQUIRE(harm != nullptr);
    REQUIRE(harm->numBars() == SaturationProcessor::kHarmonics);
    auto* sat = dynamic_cast<SaturationEditor*>(e.get());
    REQUIRE(sat != nullptr);
    const juce::String before = sat->characterDescription();
    REQUIRE(before.isNotEmpty());
    p.apvts().getParameter("character")->setValueNotifyingHost(1.0f); // Clip
    sat->refreshForTests();
    REQUIRE(sat->characterDescription() != before);
    REQUIRE(e->getWidth() == 920);
}
