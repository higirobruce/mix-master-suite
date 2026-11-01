#include "plugin_base/StateIO.h"

namespace mm::StateIO
{
juce::ValueTree snapshot(const juce::AudioProcessorValueTreeState& apvts, const ParamTable& table)
{
    juce::ValueTree params("PARAMETERS");
    for (const auto& s : table.specs())
    {
        if (auto* raw = apvts.getRawParameterValue(s.id))
        {
            juce::ValueTree p("PARAM");
            p.setProperty("id", juce::String(s.id), nullptr);
            p.setProperty("value", static_cast<double>(raw->load()), nullptr);
            params.addChild(p, -1, nullptr);
        }
    }
    return params;
}

void applySnapshot(const juce::ValueTree& params, juce::AudioProcessorValueTreeState& apvts, const ParamTable& table,
                   juce::UndoManager* undo)
{
    if (undo != nullptr)
        undo->beginNewTransaction("Recall");
    for (const auto& p : params)
    {
        if (!p.hasType("PARAM"))
            continue;
        const juce::String id = p["id"].toString();
        const auto* spec = table.find(id.toStdString());
        if (spec == nullptr)
            continue;
        auto* param = apvts.getParameter(id);
        if (param == nullptr)
            continue;
        const float real = juce::jlimit(spec->min, spec->max, static_cast<float>(static_cast<double>(p["value"])));
        param->setValueNotifyingHost(param->convertTo0to1(real));
    }
}

juce::ValueTree makePreset(const juce::AudioProcessorValueTreeState& apvts, const ParamTable& table,
                           const juce::String& name, const juce::StringArray& tags, const juce::String& author,
                           const juce::String& notes)
{
    juce::ValueTree preset("PRESET");
    preset.setProperty("name", name, nullptr);
    preset.setProperty("plugin", apvts.state.getType().toString(), nullptr);
    preset.setProperty("version", 1, nullptr);
    auto* meta = new juce::DynamicObject();
    juce::Array<juce::var> tagVars;
    for (const auto& t : tags)
        tagVars.add(t);
    meta->setProperty("tags", tagVars);
    meta->setProperty("author", author);
    meta->setProperty("notes", notes);
    preset.setProperty("meta", juce::JSON::toString(juce::var(meta), true), nullptr);
    auto params = snapshot(apvts, table);
    params = juce::ValueTree("PARAMS").createCopy();
    for (const auto& s : table.specs())
        if (auto* raw = apvts.getRawParameterValue(s.id))
        {
            juce::ValueTree p("PARAM");
            p.setProperty("id", juce::String(s.id), nullptr);
            p.setProperty("value", static_cast<double>(raw->load()), nullptr);
            params.addChild(p, -1, nullptr);
        }
    preset.addChild(params, -1, nullptr);
    return preset;
}

bool applyPreset(const juce::ValueTree& preset, juce::AudioProcessorValueTreeState& apvts, const ParamTable& table,
                 juce::UndoManager* undo)
{
    if (!preset.isValid() || !preset.hasType("PRESET"))
        return false;
    auto params = preset.getChildWithName("PARAMS");
    if (!params.isValid())
        return false;
    applySnapshot(params, apvts, table, undo);
    return true;
}

juce::StringArray tagsOf(const juce::ValueTree& preset)
{
    juce::StringArray out;
    const auto meta = juce::JSON::parse(preset["meta"].toString());
    if (auto* arr = meta.getProperty("tags", juce::var()).getArray())
        for (const auto& t : *arr)
            out.add(t.toString());
    return out;
}
} // namespace mm::StateIO
