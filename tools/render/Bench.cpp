#include "tools/render/Bench.h"
#include "tools/render/PluginTarget.h"
#include "tools/render/Signals.h"
#include <chrono>
#include <iostream>

namespace mmr
{
int runBench(const juce::String& plugin, int instances, double seconds, double sampleRate, int block)
{
    std::vector<std::unique_ptr<PluginTarget>> targets;
    juce::String error;
    Job job;
    job.sampleRate = sampleRate;
    job.blockSize = block;
    for (int i = 0; i < instances; ++i)
    {
        auto t = PluginTarget::load(plugin, error);
        if (t == nullptr || !t->prepare(sampleRate, block, job, error))
        {
            std::cerr << "bench: " << error << "\n";
            return 3;
        }
        targets.push_back(std::move(t));
    }
    auto* noiseSpec = new juce::DynamicObject();
    noiseSpec->setProperty("gen", "noise");
    noiseSpec->setProperty("amp_db", -12.0);
    noiseSpec->setProperty("dur_s", seconds);
    juce::AudioBuffer<float> input;
    if (!makeInput(juce::var(noiseSpec), sampleRate, 2, input, error))
        return 2;
    juce::AudioBuffer<float> work(2, block);
    const int blocks = input.getNumSamples() / block;
    double processSeconds = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (auto& t : targets)
        {
            for (int ch = 0; ch < 2; ++ch)
                work.copyFrom(ch, 0, input, ch, b * block, block);
            const auto t0 = std::chrono::steady_clock::now();
            t->process(work, false);
            processSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        }
    }
    const double audioSeconds = static_cast<double>(blocks * block) / sampleRate;
    const double cpuPerInstance = 100.0 * processSeconds / (static_cast<double>(instances) * audioSeconds);
    auto* o = new juce::DynamicObject();
    o->setProperty("plugin", plugin);
    o->setProperty("instances", instances);
    o->setProperty("sample_rate", sampleRate);
    o->setProperty("block_size", block);
    o->setProperty("audio_seconds", audioSeconds);
    o->setProperty("process_seconds_total", processSeconds);
    o->setProperty("cpu_percent_per_instance", cpuPerInstance);
    std::cout << juce::JSON::toString(juce::var(o)) << "\n";
    return 0;
}
} // namespace mmr
