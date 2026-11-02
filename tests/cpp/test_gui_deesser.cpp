#include "Params.h" // generated for deesser
#include "gui/GainReductionMeter.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/SibilanceDisplay.h"
#include "gui/Toggle.h"
#include "plugins/deesser/DeEsserEditor.h"
#include "plugins/deesser/DeEsserProcessor.h"
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

TEST_CASE("de-esser editor: every parameter has a control, a sibilance history that shades detected frames, a listen toggle and a GR meter")
{
    GuiInit g;
    DeEsserProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    collect(*e, ids);
    const auto& table = mm::plugins::deesser::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) != "header")
            REQUIRE(ids.count(spec.id) == 1);
    REQUIRE(ids.count("listen") == 1);
    auto* hist = find<mm::gui::SibilanceDisplay>(*e);
    REQUIRE(hist != nullptr);
    REQUIRE(find<mm::gui::GainReductionMeter>(*e) != nullptr);
    mm::MeterFrame f;
    f.peakInDb[0] = f.peakInDb[1] = -12.0f;
    hist->push(f, true, 9.0f);
    hist->push(f, false, 0.0f);
    REQUIRE(hist->numPoints() == 2);
    REQUIRE(hist->sibilantCount() == 1);
    REQUIRE(e->getWidth() == 920);
}
