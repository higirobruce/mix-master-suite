#include "Params.h" // generated for reverb
#include "gui/DecayDisplay.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "plugins/reverb/ReverbEditor.h"
#include "plugins/reverb/ReverbProcessor.h"
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

TEST_CASE("reverb editor: every parameter has a control and the decay display drags the decay time")
{
    GuiInit g;
    ReverbProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    collect(*e, ids);
    const auto& table = mm::plugins::reverb::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) != "header")
            REQUIRE(ids.count(spec.id) == 1);
    auto* decay = find<mm::gui::DecayDisplay>(*e);
    REQUIRE(decay != nullptr);
    REQUIRE(e->getWidth() == 920);
    decay->dragDecayTo(decay->xForSeconds(4.0));
    REQUIRE(p.apvts().getRawParameterValue("decay_s")->load() == Catch::Approx(4.0f).epsilon(0.05));
    // the analytic envelope reaches -60 dB at the decay time
    REQUIRE(decay->envelopeDb(4.0) == Catch::Approx(-60.0f).margin(0.5f));
    REQUIRE(decay->envelopeDb(0.0) == Catch::Approx(0.0f).margin(0.01f));
}
