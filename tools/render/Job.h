#pragma once
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace mmr
{
struct AutomationPoint
{
    juce::String param;
    double timeSeconds = 0.0;
    double value = 0.0;
};

struct ScheduleEvent
{
    double timeSeconds = 0.0;
    std::optional<int> blockSize;
    std::optional<double> sampleRate;
};

struct ParamRange // optional, supplied by the Python harness from params.json for exact real->normalised mapping
{
    float min = 0.0f, max = 1.0f, skew = 1.0f;
    juce::String type{"float"};
    int numChoices = 0;
};

struct Job
{
    juce::String plugin;  // path to .vst3 / .component
    juce::String module;  // core module name
    double sampleRate = 48000.0;
    int blockSize = 64;
    int inChannels = 2, outChannels = 2, sidechainChannels = 0;
    double sidechainDb = 0.0; // sidechain channels = input channel 0 scaled by this
    double bpm = 120.0;       // tempo reported to the plugin through the play head
    juce::var params;         // object id -> real value
    juce::var paramRanges;    // object id -> {min,max,skew,type}
    std::vector<AutomationPoint> automation;
    bool sampleRateAutomation = false;
    juce::var input;          // {"file":...} | {"gen":...}
    std::vector<ScheduleEvent> schedule;
    bool stateRoundtrip = false;
    bool bypassHost = false;
    double bypassHostAtSeconds = -1.0;
    juce::String output;
    juce::String report;
    juce::String meter;      // "loudness": BS.1770 / EBU readings of the output in the report (M5)

    static Job parse(const juce::var& v, juce::String& error);
};
} // namespace mmr
