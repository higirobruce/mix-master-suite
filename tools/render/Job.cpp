#include "tools/render/Job.h"

namespace mmr
{
Job Job::parse(const juce::var& v, juce::String& error)
{
    Job j;
    if (!v.isObject())
    {
        error = "job must be a JSON object";
        return j;
    }
    j.plugin = v.getProperty("plugin", "").toString();
    j.module = v.getProperty("module", "").toString();
    if (j.plugin.isEmpty() == j.module.isEmpty())
    {
        error = "job needs exactly one of 'plugin' or 'module'";
        return j;
    }
    j.sampleRate = static_cast<double>(v.getProperty("sample_rate", 48000.0));
    j.blockSize = static_cast<int>(v.getProperty("block_size", 64));
    const int channels = static_cast<int>(v.getProperty("channels", 2));
    j.inChannels = j.outChannels = channels;
    if (auto layout = v.getProperty("layout", juce::var()); layout.isObject())
    {
        j.inChannels = static_cast<int>(layout.getProperty("in", channels));
        j.outChannels = static_cast<int>(layout.getProperty("out", channels));
        j.sidechainChannels = static_cast<int>(layout.getProperty("sidechain", 0));
    }
    j.sidechainDb = static_cast<double>(v.getProperty("sidechain_db", 0.0));
    j.bpm = static_cast<double>(v.getProperty("bpm", 120.0));
    j.params = v.getProperty("params", juce::var());
    j.paramRanges = v.getProperty("param_ranges", juce::var());
    if (auto* arr = v.getProperty("automation", juce::var()).getArray())
        for (const auto& a : *arr)
            j.automation.push_back({a.getProperty("param", "").toString(), static_cast<double>(a.getProperty("t_s", 0.0)),
                                    static_cast<double>(a.getProperty("value", 0.0))});
    j.sampleRateAutomation = v.getProperty("automation_rate", "block").toString() == "sample";
    j.input = v.getProperty("input", juce::var());
    if (auto* arr = v.getProperty("schedule", juce::var()).getArray())
        for (const auto& s : *arr)
        {
            ScheduleEvent e;
            e.timeSeconds = static_cast<double>(s.getProperty("t_s", 0.0));
            if (s.hasProperty("block_size"))
            {
                e.blockSize = static_cast<int>(s["block_size"]);
                if (*e.blockSize < 1)
                {
                    error = "schedule block_size must be >= 1 (an empty host block is covered by the in-process tests)";
                    return j;
                }
            }
            if (s.hasProperty("sample_rate"))
                e.sampleRate = static_cast<double>(s["sample_rate"]);
            j.schedule.push_back(e);
        }
    j.stateRoundtrip = static_cast<bool>(v.getProperty("state_roundtrip", false));
    j.bypassHost = static_cast<bool>(v.getProperty("bypass_host", false));
    j.bypassHostAtSeconds = static_cast<double>(v.getProperty("bypass_host_at_s", -1.0));
    j.output = v.getProperty("output", "").toString();
    j.report = v.getProperty("report", "").toString();
    if (j.sampleRate <= 0 || j.blockSize <= 0 || j.inChannels < 1 || j.outChannels < 1)
        error = "invalid sample_rate / block_size / channels";
    return j;
}
} // namespace mmr
