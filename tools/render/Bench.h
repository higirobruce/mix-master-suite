#pragma once
#include <juce_core/juce_core.h>

namespace mmr
{
// N instances, S seconds of noise at sr/block; prints JSON {cpu_percent_per_instance, ...}. Returns exit code.
int runBench(const juce::String& plugin, int instances, double seconds, double sampleRate, int block);
} // namespace mmr
