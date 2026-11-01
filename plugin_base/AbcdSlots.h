#pragma once
#include "plugin_base/ParamTable.h"
#include <array>

namespace mm
{
// Four parameter snapshots (A/B/C/D) plus the active slot. store() snapshots the current parameters into a slot;
// recall() switches to a slot, storing the current values into the previously active slot first.
class AbcdSlots
{
public:
    AbcdSlots(juce::AudioProcessorValueTreeState& apvts, const ParamTable& table, juce::UndoManager* undo);

    void store(int slot);
    void recall(int slot);
    void copyTo(int slot); // copy the current parameters into another slot without switching
    int active() const noexcept { return activeSlot; }
    static constexpr int kNumSlots = 4;

    juce::ValueTree toState() const;
    void fromState(const juce::ValueTree& slots);

private:
    juce::AudioProcessorValueTreeState& apvts;
    const ParamTable& table;
    juce::UndoManager* undo;
    std::array<juce::ValueTree, kNumSlots> snapshots;
    int activeSlot = 0;
};
} // namespace mm
