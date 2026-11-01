// Times N in-process instances of UtilityProcessor on 64-sample blocks; prints JSON like mm_render --bench.
#include "plugins/utility/UtilityProcessor.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <vector>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    const int instances = argc > 1 ? std::atoi(argv[1]) : 100;
    const double seconds = argc > 2 ? std::atof(argv[2]) : 5.0;
    const double sr = 48000.0;
    const int block = 64;
    std::vector<std::unique_ptr<UtilityProcessor>> procs;
    for (int i = 0; i < instances; ++i)
    {
        auto p = std::make_unique<UtilityProcessor>();
        p->setNonRealtime(true);
        p->prepareToPlay(sr, block);
        procs.push_back(std::move(p));
    }
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> d(-0.25f, 0.25f);
    juce::AudioBuffer<float> work(2, block);
    juce::MidiBuffer midi;
    const int blocks = static_cast<int>(seconds * sr / block);
    double processSeconds = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (auto& p : procs)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < block; ++n)
                    work.setSample(ch, n, d(rng));
            const auto t0 = std::chrono::steady_clock::now();
            p->processBlock(work, midi);
            processSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        }
    }
    const double audioSeconds = blocks * block / sr;
    std::printf("{\"instances\": %d, \"cpu_percent_per_instance\": %.5f, \"ns_per_block\": %.1f}\n", instances,
                100.0 * processSeconds / (instances * audioSeconds), 1e9 * processSeconds / (instances * blocks));
    return 0;
}
