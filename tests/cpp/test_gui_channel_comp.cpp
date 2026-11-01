#include "Params.h" // generated for channel_comp
#include "gui/GainReductionMeter.h"
#include "gui/HistoryDisplay.h"
#include "gui/Knob.h"
#include "gui/Selector.h"
#include "gui/Toggle.h"
#include "gui/TransferCurveDisplay.h"
#include "plugins/channel_comp/ChannelCompEditor.h"
#include "plugins/channel_comp/ChannelCompProcessor.h"
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

TEST_CASE("channel compressor editor: every parameter has a control; transfer curve, history and GR meter present")
{
    GuiInit g;
    ChannelCompProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    std::set<std::string> ids;
    collect(*e, ids);
    const auto& table = mm::plugins::channel_comp::table();
    for (const auto& id : ids)
        REQUIRE(table.find(id) != nullptr);
    for (const auto& spec : table.specs())
        if (std::string(spec.group) != "header")
            REQUIRE(ids.count(spec.id) == 1);
    REQUIRE(find<mm::gui::TransferCurveDisplay>(*e) != nullptr);
    REQUIRE(find<mm::gui::HistoryDisplay>(*e) != nullptr);
    REQUIRE(find<mm::gui::GainReductionMeter>(*e) != nullptr);
    REQUIRE(e->getWidth() == 920);
}

TEST_CASE("transfer curve maps the static gain computer and the threshold can be dragged on the history")
{
    GuiInit g;
    ChannelCompProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    auto* curve = find<mm::gui::TransferCurveDisplay>(*e);
    REQUIRE(curve != nullptr);
    // input -10 dB with threshold -20, 4:1, knee 6 (defaults) -> output -17.5 dB
    REQUIRE(curve->outputDbFor(-10.0f) == Catch::Approx(-17.5f).margin(0.05f));
    REQUIRE(curve->outputDbFor(-40.0f) == Catch::Approx(-40.0f).margin(0.01f));
    auto* hist = find<mm::gui::HistoryDisplay>(*e);
    REQUIRE(hist != nullptr);
    const float before = p.apvts().getRawParameterValue("threshold_db")->load();
    hist->dragThresholdTo(hist->yForDb(-30.0f));
    REQUIRE(p.apvts().getRawParameterValue("threshold_db")->load() == Catch::Approx(-30.0f).margin(0.5f));
    REQUIRE(before != p.apvts().getRawParameterValue("threshold_db")->load());
    // history accepts frames
    mm::MeterFrame f;
    f.gainReductionDb = 6.0f;
    f.peakInDb[0] = f.peakInDb[1] = -6.0f;
    hist->push(f);
    REQUIRE(hist->numPoints() == 1);
}
