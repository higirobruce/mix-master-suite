#pragma once
#include "plugin_base/ParamTable.h"
#include <vector>

namespace mm
{
// Factory presets come from the plugin's BinaryData (passed as a FactorySource so this library does not depend on a
// generated namespace); user presets live in userDir(pluginId). Preset file format: ADR-0002.
class PresetManager
{
public:
    struct FactorySource
    {
        const char* (*getNamedResource)(const char* name, int& size) = nullptr;
        const char* const* names = nullptr;
        int count = 0;
    };

    struct Info
    {
        juce::String name;
        juce::StringArray tags;
        juce::StringArray paramIds;
        juce::File file; // empty for factory presets
        bool factory = false;
        juce::String xml; // factory preset contents
    };

    PresetManager(juce::AudioProcessorValueTreeState& apvts, const ParamTable& table, const juce::String& pluginId,
                  juce::UndoManager* undo, FactorySource factory);

    std::vector<Info> list() const;
    bool load(const Info& info);
    bool save(const juce::String& name, const juce::StringArray& tags);
    bool loadFromValueTree(const juce::ValueTree& preset, const juce::String& displayName);
    juce::String currentName() const { return current; }
    void setCurrentName(const juce::String& name) { current = name; }

    static juce::File userDir(const juce::String& pluginId);

private:
    juce::AudioProcessorValueTreeState& apvts;
    const ParamTable& table;
    juce::String pluginId;
    juce::UndoManager* undo;
    FactorySource factory;
    juce::String current{"Default"};
};
} // namespace mm
