#include "plugin_base/PresetManager.h"
#include "plugin_base/StateIO.h"

namespace mm
{
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& s, const ParamTable& t, const juce::String& id,
                             juce::UndoManager* u, FactorySource f)
    : apvts(s), table(t), pluginId(id), undo(u), factory(f)
{
}

juce::File PresetManager::userDir(const juce::String& id)
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("MixMaster")
        .getChildFile("Presets")
        .getChildFile(id);
}

static PresetManager::Info infoFromTree(const juce::ValueTree& tree, const juce::String& fallbackName)
{
    PresetManager::Info info;
    info.name = tree.getProperty("name", fallbackName).toString();
    info.tags = StateIO::tagsOf(tree);
    for (const auto& p : tree.getChildWithName("PARAMS"))
        if (p.hasType("PARAM"))
            info.paramIds.add(p["id"].toString());
    return info;
}

std::vector<PresetManager::Info> PresetManager::list() const
{
    std::vector<Info> out;
    for (int i = 0; i < factory.count; ++i)
    {
        const juce::String resName(factory.names[i]);
        if (!resName.endsWith("_mmpreset"))
            continue;
        int size = 0;
        const char* data = factory.getNamedResource(factory.names[i], size);
        if (data == nullptr)
            continue;
        const juce::String xml(juce::CharPointer_UTF8(data), static_cast<size_t>(size));
        auto tree = juce::ValueTree::fromXml(xml);
        if (!tree.isValid())
            continue;
        auto info = infoFromTree(tree, resName.upToLastOccurrenceOf("_mmpreset", false, false));
        info.factory = true;
        info.xml = xml;
        out.push_back(std::move(info));
    }
    const auto dir = userDir(pluginId);
    if (dir.isDirectory())
    {
        for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.mmpreset"))
        {
            auto tree = juce::ValueTree::fromXml(f.loadFileAsString());
            if (!tree.isValid())
                continue;
            auto info = infoFromTree(tree, f.getFileNameWithoutExtension());
            info.file = f;
            out.push_back(std::move(info));
        }
    }
    return out;
}

bool PresetManager::loadFromValueTree(const juce::ValueTree& preset, const juce::String& displayName)
{
    if (!StateIO::applyPreset(preset, apvts, table, undo))
        return false;
    current = displayName;
    return true;
}

bool PresetManager::load(const Info& info)
{
    const juce::String xml = info.factory ? info.xml : info.file.loadFileAsString();
    auto tree = juce::ValueTree::fromXml(xml);
    return loadFromValueTree(tree, info.name);
}

bool PresetManager::save(const juce::String& name, const juce::StringArray& tags)
{
    auto preset = StateIO::makePreset(apvts, table, name, tags);
    preset.setProperty("plugin", pluginId, nullptr);
    const auto dir = userDir(pluginId);
    if (!dir.createDirectory())
        return false;
    const auto file = dir.getChildFile(juce::File::createLegalFileName(name) + ".mmpreset");
    if (!file.replaceWithText(preset.toXmlString()))
        return false;
    current = name;
    return true;
}
} // namespace mm
