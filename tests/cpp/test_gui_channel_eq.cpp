#include "Params.h" // generated for channel_eq
#include "gui/CurveDisplay.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "plugins/channel_eq/ChannelEqEditor.h"
#include "plugins/channel_eq/ChannelEqProcessor.h"
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
mm::gui::CurveDisplay* findCurve(juce::Component& c)
{
    if (auto* d = dynamic_cast<mm::gui::CurveDisplay*>(&c)) return d;
    for (auto* child : c.getChildren())
        if (auto* d = findCurve(*child)) return d;
    return nullptr;
}
} // namespace

TEST_CASE("channel eq editor creates, has a control for every band/filter/advanced parameter and a curve")
{
    GuiInit g;
    ChannelEqProcessor p;
    for (int i = 0; i < 10; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
        REQUIRE(e != nullptr);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    int curves = 0;
    collect(*e, ids, curves);
    REQUIRE(curves == 1);
    const auto& table = mm::plugins::channel_eq::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
    {
        const std::string group(spec.group);
        if (group == "band" || group == "filters" || group == "advanced" || group == "footer")
            REQUIRE(ids.count(spec.id) == 1);
    }
    REQUIRE(e->getWidth() == 920);
    REQUIRE(e->getHeight() == 580);
}

TEST_CASE("curve display shows one node per enabled band and dragging a node moves the band")
{
    GuiInit g;
    ChannelEqProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    auto* curve = findCurve(*e);
    REQUIRE(curve != nullptr);
    REQUIRE(curve->numNodes() == 5);
    auto* enabled = p.apvts().getParameter("b2_enabled");
    enabled->setValueNotifyingHost(0.0f);
    curve->refreshFromProcessor();
    REQUIRE(curve->numNodes() == 4);
    enabled->setValueNotifyingHost(1.0f);
    curve->refreshFromProcessor();
    REQUIRE(curve->numNodes() == 5);
    // node drag: move band 3 node by +40 px up (gain up) and +60 px right (freq up)
    const auto before = curve->nodePosition(2);
    curve->dragNode(2, before.translated(60.0f, -40.0f));
    const float freq = p.apvts().getRawParameterValue("b3_freq_hz")->load();
    const float gain = p.apvts().getRawParameterValue("b3_gain_db")->load();
    REQUIRE(freq > 1000.0f);
    REQUIRE(gain > 0.0f);
    // wheel changes Q
    const float q0 = p.apvts().getRawParameterValue("b3_q")->load();
    curve->wheelNode(2, 1.0f);
    REQUIRE(p.apvts().getRawParameterValue("b3_q")->load() != q0);
}
