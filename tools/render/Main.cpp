// mm_render: offline plugin/module host for the measurement suite.
//   mm_render <job.json>
//   mm_render --dump-params <plugin>
//   mm_render --bench <plugin>|module:<name> --instances N --seconds S --sample-rate R --block B [--set id=value]...
//   mm_render --list-modules
// Exit codes: 0 ok, 2 job error, 3 plugin load failure.
#include "core/metering/LoudnessMeter.h"
#include "core/metering/TruePeakMeter.h"
#include "tools/render/Bench.h"
#include "tools/render/Job.h"
#include "tools/render/ModuleRegistry.h"
#include "tools/render/PluginTarget.h"
#include "tools/render/Signals.h"
#include <juce_events/juce_events.h>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>

using namespace mmr;

namespace
{
int renderJob(const juce::File& jobFile)
{
    juce::String error;
    const auto jobVar = juce::JSON::parse(jobFile.loadFileAsString());
    Job job = Job::parse(jobVar, error);
    if (error.isNotEmpty())
    {
        std::cerr << "job: " << error << "\n";
        return 2;
    }

    std::unique_ptr<Target> target;
    if (job.plugin.isNotEmpty())
    {
        target = PluginTarget::load(job.plugin, error);
        if (target == nullptr)
        {
            std::cerr << "plugin: " << error << "\n";
            return 3;
        }
    }
    else
    {
        auto m = ModuleRegistry::instance().create(job.module.toStdString());
        if (m == nullptr)
        {
            std::cerr << "module: unknown module " << job.module << "\n";
            return 2;
        }
        target = std::make_unique<ModuleTarget>(std::move(m));
    }

    juce::AudioBuffer<float> input;
    if (!makeInput(job.input, job.sampleRate, job.inChannels, input, error))
    {
        std::cerr << "input: " << error << "\n";
        return 2;
    }
    const int totalSamples = input.getNumSamples();
    const int maxBlock = 4096 > job.blockSize ? 4096 : job.blockSize;

    // parameters before prepare, so smoothers snap
    if (job.params.isObject())
        for (const auto& kv : job.params.getDynamicObject()->getProperties())
            if (!target->setParam(kv.name.toString(), static_cast<double>(kv.value), job.paramRanges, error))
            {
                std::cerr << "param: " << error << "\n";
                return 2;
            }
    if (!target->prepare(job.sampleRate, maxBlock, job, error))
    {
        std::cerr << "prepare: " << error << "\n";
        return 3;
    }
    if (job.stateRoundtrip && !target->stateRoundtrip(error))
    {
        std::cerr << "state: " << error << "\n";
        return 3;
    }

    // host buffer: main inputs, then sidechain channels (fed with input channel 0 until a job key says otherwise)
    const int channels = juce::jmax(job.inChannels + job.sidechainChannels, job.outChannels);
    juce::AudioBuffer<float> output(job.outChannels, totalSamples);
    output.clear();
    juce::AudioBuffer<float> work(channels, maxBlock);

    double sr = job.sampleRate;
    int block = job.sampleRateAutomation ? 1 : job.blockSize;
    size_t nextAuto = 0, nextSched = 0;
    std::sort(job.automation.begin(), job.automation.end(),
              [](const AutomationPoint& a, const AutomationPoint& b) { return a.timeSeconds < b.timeSeconds; });
    std::sort(job.schedule.begin(), job.schedule.end(),
              [](const ScheduleEvent& a, const ScheduleEvent& b) { return a.timeSeconds < b.timeSeconds; });

    long nanCount = 0;
    float peak = 0.0f;
    double processSeconds = 0.0;
    int pos = 0;
    while (pos < totalSamples)
    {
        const double t = pos / job.sampleRate; // time in the input's clock
        while (nextSched < job.schedule.size() && job.schedule[nextSched].timeSeconds <= t)
        {
            const auto& e = job.schedule[nextSched++];
            if (e.blockSize)
                block = *e.blockSize;
            if (e.sampleRate && *e.sampleRate != sr)
            {
                sr = *e.sampleRate;
                target->release();
                if (!target->prepare(sr, maxBlock, job, error))
                {
                    std::cerr << "prepare: " << error << "\n";
                    return 3;
                }
            }
        }
        while (nextAuto < job.automation.size() && job.automation[nextAuto].timeSeconds <= t)
        {
            const auto& a = job.automation[nextAuto++];
            if (!target->setParam(a.param, a.value, job.paramRanges, error))
            {
                std::cerr << "automation: " << error << "\n";
                return 2;
            }
        }
        const int n = juce::jmin(block, totalSamples - pos, maxBlock);
        for (int ch = 0; ch < channels; ++ch)
        {
            if (ch < job.inChannels)
                work.copyFrom(ch, 0, input, ch, pos, n);
            else if (ch < job.inChannels + job.sidechainChannels)
                work.copyFrom(ch, 0, input.getReadPointer(0, pos), n, static_cast<float>(std::pow(10.0, job.sidechainDb / 20.0)));
            else
                work.clear(ch, 0, n);
        }
        juce::AudioBuffer<float> view(work.getArrayOfWritePointers(), channels, 0, n);
        const bool bypass = job.bypassHost || (job.bypassHostAtSeconds >= 0.0 && t >= job.bypassHostAtSeconds);
        const auto t0 = std::chrono::steady_clock::now();
        target->process(view, bypass);
        processSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        for (int ch = 0; ch < job.outChannels; ++ch)
        {
            float* dst = output.getWritePointer(ch, pos);
            const float* src = view.getReadPointer(juce::jmin(ch, channels - 1));
            for (int i = 0; i < n; ++i)
            {
                float v = src[i];
                if (!std::isfinite(v))
                {
                    ++nanCount;
                    v = 0.0f;
                }
                peak = juce::jmax(peak, std::abs(v));
                dst[i] = v;
            }
        }
        pos += n;
    }

    if (job.output.isNotEmpty() && !writeWav(juce::File(job.output), output, job.sampleRate))
    {
        std::cerr << "output: cannot write " << job.output << "\n";
        return 2;
    }
    if (job.report.isNotEmpty())
    {
        auto* o = new juce::DynamicObject();
        const int latency = target->latencySamples();
        o->setProperty("latency_samples", latency);
        o->setProperty("latency_ms", 1000.0 * latency / sr);
        o->setProperty("nan_count", static_cast<juce::int64>(nanCount));
        o->setProperty("peak_db", peak > 0.0f ? 20.0 * std::log10(static_cast<double>(peak)) : -200.0);
        o->setProperty("process_time_s", processSeconds);
        const double audioSeconds = totalSamples / job.sampleRate;
        o->setProperty("audio_time_s", audioSeconds);
        o->setProperty("realtime_ratio", audioSeconds > 0 ? processSeconds / audioSeconds : 0.0);
        o->setProperty("params", target->paramsDump());
        if (job.meter == "loudness")
        {
            // BS.1770-4 / EBU readings of the rendered output (core meters, offline, 1024-sample chunks)
            mm::LoudnessMeter lm;
            mm::TruePeakMeter tp;
            lm.prepare(job.sampleRate, job.outChannels);
            tp.prepare(job.sampleRate, job.outChannels, 1024);
            std::vector<const float*> ptrs(static_cast<size_t>(job.outChannels));
            for (int p = 0; p < totalSamples; p += 1024)
            {
                const int n = juce::jmin(1024, totalSamples - p);
                for (int ch = 0; ch < job.outChannels; ++ch)
                    ptrs[static_cast<size_t>(ch)] = output.getReadPointer(ch, p);
                lm.process(ptrs.data(), n);
                tp.processBlock(ptrs.data(), n);
            }
            auto* m = new juce::DynamicObject();
            m->setProperty("lufs_i", lm.integratedLufs());
            m->setProperty("lufs_m_max", lm.maxMomentaryLufs());
            m->setProperty("lufs_s_max", lm.maxShortTermLufs());
            m->setProperty("lra", lm.loudnessRangeLu());
            m->setProperty("true_peak_dbtp", tp.maxDbtp());
            m->setProperty("true_peak_oversampling", tp.oversamplingFactor());
            o->setProperty("meter", juce::var(m));
        }
        juce::File(job.report).replaceWithText(juce::JSON::toString(juce::var(o)));
    }
    target->release();
    return 0;
}
} // namespace

int main(int argc, char* argv[])
{
#if ! JUCE_WINDOWS
    // a closed pipe (the test harness, a dying X server on a CI runner) surfaces as a write error, not a kill:
    // one Linux CI render died with signal 13 inside a plugin's base checks (run 41785906260)
    std::signal(SIGPIPE, SIG_IGN);
#endif
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();
    registerCoreModules();

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add(juce::String(juce::CharPointer_UTF8(argv[i])));
    if (args.isEmpty())
    {
        std::cerr << "usage: mm_render <job.json> | --dump-params <plugin> | --bench <plugin> [--instances N] "
                     "[--seconds S] [--sample-rate R] [--block B] | --list-modules\n";
        return 2;
    }
    if (args[0] == "--list-modules")
    {
        juce::Array<juce::var> names;
        for (const auto& n : ModuleRegistry::instance().names())
            names.add(juce::String(n));
        std::cout << juce::JSON::toString(juce::var(names)) << "\n";
        return 0;
    }
    if (args[0] == "--dump-params")
    {
        if (args.size() < 2)
            return 2;
        juce::String error;
        auto t = PluginTarget::load(args[1], error);
        if (t == nullptr)
        {
            std::cerr << error << "\n";
            return 3;
        }
        std::cout << juce::JSON::toString(t->paramsDump()) << "\n";
        return 0;
    }
    if (args[0] == "--bench")
    {
        if (args.size() < 2)
            return 2;
        int instances = 100, block = 64;
        juce::StringArray params;
        double seconds = 10.0, sampleRate = 48000.0;
        for (int i = 2; i + 1 < args.size(); i += 2)
        {
            if (args[i] == "--instances")
                instances = args[i + 1].getIntValue();
            else if (args[i] == "--set")
                params.add(args[i + 1]);
            else if (args[i] == "--seconds")
                seconds = args[i + 1].getDoubleValue();
            else if (args[i] == "--sample-rate")
                sampleRate = args[i + 1].getDoubleValue();
            else if (args[i] == "--block")
                block = args[i + 1].getIntValue();
        }
        return runBench(args[1], instances, seconds, sampleRate, block, params);
    }
    return renderJob(juce::File::getCurrentWorkingDirectory().getChildFile(args[0]));
}
