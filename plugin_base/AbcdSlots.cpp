#include "plugin_base/AbcdSlots.h"
#include "plugin_base/StateIO.h"

namespace mm
{
AbcdSlots::AbcdSlots(juce::AudioProcessorValueTreeState& s, const ParamTable& t, juce::UndoManager* u)
    : apvts(s), table(t), undo(u)
{
}

void AbcdSlots::store(int slot)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    snapshots[static_cast<size_t>(slot)] = StateIO::snapshot(apvts, table);
}

void AbcdSlots::copyTo(int slot) { store(slot); }

void AbcdSlots::recall(int slot)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    store(activeSlot);
    activeSlot = slot;
    const auto& snap = snapshots[static_cast<size_t>(slot)];
    if (snap.isValid())
        StateIO::applySnapshot(snap, apvts, table, undo);
}

juce::ValueTree AbcdSlots::toState() const
{
    juce::ValueTree slots("SLOTS");
    slots.setProperty("active", activeSlot, nullptr);
    for (int i = 0; i < kNumSlots; ++i)
    {
        juce::ValueTree slot("SLOT");
        slot.setProperty("index", i, nullptr);
        if (snapshots[static_cast<size_t>(i)].isValid())
            slot.addChild(snapshots[static_cast<size_t>(i)].createCopy(), -1, nullptr);
        slots.addChild(slot, -1, nullptr);
    }
    return slots;
}

void AbcdSlots::fromState(const juce::ValueTree& slots)
{
    if (!slots.isValid())
        return;
    activeSlot = juce::jlimit(0, kNumSlots - 1, static_cast<int>(slots.getProperty("active", 0)));
    for (const auto& slot : slots)
    {
        const int i = static_cast<int>(slot.getProperty("index", -1));
        if (i < 0 || i >= kNumSlots)
            continue;
        auto params = slot.getChildWithName("PARAMETERS");
        snapshots[static_cast<size_t>(i)] = params.isValid() ? params.createCopy() : juce::ValueTree();
    }
}
} // namespace mm
