#pragma once
#include "plugin_base/ParamTable.h"

namespace mm::StateIO
{
// Preset ValueTree (ADR-0002): PRESET{name, plugin, version, meta(JSON)} / PARAMS / PARAM{id, value}
juce::ValueTree makePreset(const juce::AudioProcessorValueTreeState& apvts, const ParamTable& table,
                           const juce::String& name, const juce::StringArray& tags,
                           const juce::String& author = "MixMaster", const juce::String& notes = {});

// Apply a preset: unknown ids ignored, missing ids untouched. Returns false if the tree is not a PRESET.
bool applyPreset(const juce::ValueTree& preset, juce::AudioProcessorValueTreeState& apvts, const ParamTable& table,
                 juce::UndoManager* undo);

// Parameter snapshot used by A/B/C/D slots and state: PARAMETERS / PARAM{id, value}.
juce::ValueTree snapshot(const juce::AudioProcessorValueTreeState& apvts, const ParamTable& table);
void applySnapshot(const juce::ValueTree& params, juce::AudioProcessorValueTreeState& apvts, const ParamTable& table,
                   juce::UndoManager* undo);

juce::StringArray tagsOf(const juce::ValueTree& preset);
} // namespace mm::StateIO
