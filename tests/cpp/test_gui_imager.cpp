#include "Params.h" // generated for imager
#include "gui/Goniometer.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/SpectrumDisplay.h"
#include "gui/Toggle.h"
#include "plugins/imager/ImagerEditor.h"
#include "plugins/imager/ImagerProcessor.h"
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

TEST_CASE("imager editor: every parameter has a control, a goniometer fed from the scope with a correlation bar, and a spectrum with the mono-below line")
{
    GuiInit g;
    ImagerProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    collect(*e, ids);
    const auto& table = mm::plugins::imager::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) != "header")
            REQUIRE(ids.count(spec.id) == 1);
    auto* gonio = find<mm::gui::Goniometer>(*e);
    REQUIRE(gonio != nullptr);
    const float l[4] = {0.5f, -0.5f, 0.25f, 0.0f}, r[4] = {0.5f, -0.5f, -0.25f, 0.0f};
    gonio->pushScope(l, r, 4);
    REQUIRE(gonio->numPoints() == 4);
    gonio->setCorrelation(-0.5f);
    REQUIRE(gonio->correlation() == Catch::Approx(-0.5f));
    auto* spectrum = find<mm::gui::SpectrumDisplay>(*e);
    REQUIRE(spectrum != nullptr);
    spectrum->setMarkerHz(200.0f);
    REQUIRE(spectrum->markerHz() == Catch::Approx(200.0f));
    REQUIRE(e->getWidth() == 920);
}
