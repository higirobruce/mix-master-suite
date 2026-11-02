#pragma once
#include <juce_core/juce_core.h>

namespace mmr
{
// N instances, S seconds of noise at sr/block; prints JSON {cpu_percent_per_instance, ...}. Returns exit code.
// `plugin` is a bundle path, or "module:<name>" for a core module (instances share the host process like plugins).
// `params` are id=value pairs applied to every instance.
int runBench(const juce::String& plugin, int instances, double seconds, double sampleRate, int block,
             const juce::StringArray& params = {});
} // namespace mmr
