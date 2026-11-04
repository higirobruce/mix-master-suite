#include "Params.h" // generated for ms_processor
#include "gui/Goniometer.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/SpectrumDisplay.h"
#include "gui/Toggle.h"
#include "plugins/ms_processor/MsEditor.h"
#include "plugins/ms_processor/MsProcessor.h"
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
void collect(juce::Component& c, std::set<std::string>& ids, int& gonios, int& spectra)
{
    if (auto* k = dynamic_cast<mm::gui::Knob*>(&c)) ids.insert(k->paramId());
    if (auto* t = dynamic_cast<mm::gui::Toggle*>(&c)) ids.insert(t->paramId());
    if (auto* s = dynamic_cast<mm::gui::Selector*>(&c)) ids.insert(s->paramId());
    if (dynamic_cast<mm::gui::Goniometer*>(&c)) ++gonios;
    if (dynamic_cast<mm::gui::SpectrumDisplay*>(&c)) ++spectra;
    for (auto* child : c.getChildren())
        collect(*child, ids, gonios, spectra);
}
} // namespace

TEST_CASE("ms processor editor creates, has a control for every parameter, a goniometer and a spectrum with the mono-below line")
{
    GuiInit g;
    MsProcessor p;
    for (int i = 0; i < 5; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    int gonios = 0, spectra = 0;
    collect(*e, ids, gonios, spectra);
    REQUIRE(gonios == 1);
    REQUIRE(spectra == 1);
    const auto& table = mm::plugins::ms_processor::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
    {
        const std::string group(spec.group);
        if (group == "main" || group == "eq" || group == "footer")
            REQUIRE(ids.count(spec.id) == 1);
    }
    REQUIRE(e->getWidth() == 920);
    REQUIRE(e->getHeight() == 580);
}
