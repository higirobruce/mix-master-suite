#include "plugins/soft_clipper/SoftClipperProcessor.h"
#include "Params.h"
#include "core/common/Math.h"
#include "core/nonlinear/ADAA.h"
#include "core/nonlinear/Waveshapers.h"
#include "plugins/soft_clipper/SoftClipperEditor.h"
#include "soft_clipper_data.h"
#include <algorithm>
#include <cmath>

namespace sc = mm::plugins::soft_clipper;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &soft_clipper_data::getNamedResource;
    f.names = soft_clipper_data::namedResourceList;
    f.count = soft_clipper_data::namedResourceListSize;
    return f;
}
} // namespace

SoftClipperProcessor::SoftClipperProcessor()
    : mm::PluginProcessorBase(sc::table(), sc::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), sc::kPluginName)
{
    input = &smoothed(sc::id::input_db);
    ceiling = &smoothed(sc::id::ceiling_db);
    knee = &smoothed(sc::id::knee_pct);
    mix = &smoothed(sc::id::mix_pct);
    oversampleRef = paramRef(sc::id::oversample);
    truePeakRef = paramRef(sc::id::true_peak);
    deltaRef = paramRef(sc::id::listen_delta);
}

juce::AudioProcessorEditor* SoftClipperProcessor::createEditor() { return new SoftClipperEditor(*this); }

float SoftClipperProcessor::staticGainDb(float levelDb) noexcept
{
    const float c = mm::dbToGain(apvts().getRawParameterValue(sc::id::ceiling_db)->load());
    const float k = apvts().getRawParameterValue(sc::id::knee_pct)->load() * 0.01f;
    const float x = mm::dbToGain(levelDb);
    const float y = c * mm::shape::softClip(x / c, k);
    return x > 0.0f ? mm::gainToDb(y / x) : 0.0f;
}

void SoftClipperProcessor::prepareDsp(double sampleRate, int maxBlock, int channels)
{
    sr = sampleRate;
    for (int i = 0; i < kPaths; ++i)
    {
        paths[static_cast<size_t>(i)].os.prepare(sr, maxBlock, kChannels, i == 0 ? 4 : 8);
        paths[static_cast<size_t>(i)].reset();
    }
    maxLatency = std::max(paths[0].os.latencySamples(), paths[1].os.latencySamples());
    dryDelay.prepare(maxLatency, kChannels, maxBlock);
    for (auto* v : {&wetA, &wetB, &dry})
        for (auto& b : *v)
            b.assign(static_cast<size_t>(maxBlock), 0.0f);
    snapSmoothersToTargets();
    activePath = oversampleRef.on() ? 1 : 0;
    pendingPath = -1;
    warmupRemaining = fadeRemaining = 0;
    reportedLatency = paths[static_cast<size_t>(activePath)].os.latencySamples();
    dryDelay.setDelay(reportedLatency);
    rampsValid = false;
    resetDsp();
    juce::ignoreUnused(channels);
}

void SoftClipperProcessor::resetDsp()
{
    for (auto& p : paths)
        p.reset();
    dryDelay.reset();
}

void SoftClipperProcessor::beginBlock(int)
{
    const int wanted = oversampleRef.on() ? 1 : 0;
    if (wanted != activePath && pendingPath < 0)
    {
        pendingPath = wanted;
        paths[static_cast<size_t>(wanted)].reset();
        warmupRemaining = paths[static_cast<size_t>(wanted)].os.latencySamples();
        fadeTotal = std::max(1, static_cast<int>(std::lround(kFadeMs * 1e-3 * sr)));
        fadeRemaining = fadeTotal;
    }
}

void SoftClipperProcessor::runPath(Path& path, float* const* out, int n, float cStart, float cEnd, float kStart, float kEnd, float& maxClip) noexcept
{
    juce::dsp::AudioBlock<float> block(out, static_cast<size_t>(kChannels), 0, static_cast<size_t>(n));
    auto up = path.os.up(block);
    const int m = static_cast<int>(up.getNumSamples());
    for (int ch = 0; ch < kChannels; ++ch)
    {
        float* p = up.getChannelPointer(static_cast<size_t>(ch));
        double& x1 = path.x1[static_cast<size_t>(ch)];
        for (int i = 0; i < m; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(m);
            const float c = cStart + (cEnd - cStart) * t, k = kStart + (kEnd - kStart) * t;
            const float x = p[i];
            // the shape scaled to the ceiling: y = c * clip(x / c), antiderivative c^2 * AD1(x / c)
            const float y = mm::adaa1Step([c, k](float v) { return c * mm::shape::softClip(v / c, k); },
                                          [c, k](double v) { return static_cast<double>(c) * static_cast<double>(c) * mm::shape::softClipAD1D(v / c, k); }, x, x1);
            maxClip = std::max(maxClip, std::abs(x) - std::abs(y));
            p[i] = y;
        }
    }
    path.os.down(block);
}

void SoftClipperProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), kChannels);
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), kChannels);
    if (numMain <= 0 || numOut <= 0)
        return;
    const float inStartDb = input->current(), cStartDb = ceiling->current(), kStartPct = knee->current(), mixStartPct = mix->current();
    for (auto* s : {input, ceiling, knee, mix})
        s->skip(n);
    const bool truePeak = truePeakRef.on();
    const float marginA = truePeak ? mm::dbToGain(-kTruePeakMarginDb[0]) : 1.0f, marginB = truePeak ? mm::dbToGain(-kTruePeakMarginDb[1]) : 1.0f;
    float inStart = mm::dbToGain(inStartDb), inEnd = mm::dbToGain(input->current());
    float cStart = mm::dbToGain(cStartDb), cEnd = mm::dbToGain(ceiling->current());
    float kStart = kStartPct * 0.01f, kEnd = knee->current() * 0.01f;
    float mixStart = mixStartPct * 0.01f, mixEnd = mix->current() * 0.01f;
    if (!rampsValid)
    {
        inStart = inEnd;
        cStart = cEnd;
        kStart = kEnd;
        mixStart = mixEnd;
        rampsValid = true;
    }
    // input gain into the wet lanes; the dry side delayed to the reported latency
    const float* in[kChannels] = {buffer.getReadPointer(0), buffer.getReadPointer(numMain > 1 ? 1 : 0)};
    float* a[kChannels] = {wetA[0].data(), wetA[1].data()};
    float* b[kChannels] = {wetB[0].data(), wetB[1].data()};
    for (int ch = 0; ch < kChannels; ++ch)
    {
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(n);
            const float g = inStart + (inEnd - inStart) * t;
            a[ch][i] = in[ch][i] * g;
            b[ch][i] = a[ch][i];
        }
        dryDelay.writeBlock(a[ch], n, ch);
        dryDelay.readBlock(dry[static_cast<size_t>(ch)].data(), n, dryDelay.getDelay(), ch);
    }
    float maxClip = 0.0f;
    const float mA = activePath == 0 ? marginA : marginB;
    runPath(paths[static_cast<size_t>(activePath)], a, n, cStart * mA, cEnd * mA, kStart, kEnd, maxClip);
    if (pendingPath >= 0)
    {
        const float mB = pendingPath == 0 ? marginA : marginB;
        float clipB = 0.0f;
        runPath(paths[static_cast<size_t>(pendingPath)], b, n, cStart * mB, cEnd * mB, kStart, kEnd, clipB);
        for (int i = 0; i < n; ++i)
        {
            if (warmupRemaining > 0)
            {
                --warmupRemaining;
                continue;
            }
            const float t = 1.0f - static_cast<float>(fadeRemaining) / static_cast<float>(fadeTotal);
            for (int ch = 0; ch < kChannels; ++ch)
                a[ch][i] += (b[ch][i] - a[ch][i]) * t;
            if (fadeRemaining > 0)
                --fadeRemaining;
        }
        if (warmupRemaining <= 0 && fadeRemaining <= 0)
        {
            activePath = pendingPath;
            pendingPath = -1;
            reportedLatency = paths[static_cast<size_t>(activePath)].os.latencySamples();
            dryDelay.setDelay(reportedLatency);
            maxClip = std::max(maxClip, clipB);
        }
    }
    const bool deltaOn = deltaRef.on(); // `delta` is a base-class member (GCC -Wshadow)
    for (int ch = 0; ch < numOut; ++ch)
    {
        float* outPtr = buffer.getWritePointer(ch);
        const float* w = a[juce::jmin(ch, kChannels - 1)];
        const float* d = dry[static_cast<size_t>(juce::jmin(ch, kChannels - 1))].data();
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(n);
            const float c = cStart + (cEnd - cStart) * t;
            float wet = w[i];
            if (!truePeak)
                wet = juce::jlimit(-c, c, wet); // sample peaks exactly at the ceiling
            const float mx = mixStart + (mixEnd - mixStart) * t;
            outPtr[i] = deltaOn ? d[i] - wet : d[i] + (wet - d[i]) * mx;
        }
    }
    clipDb.store(maxClip > 0.0f ? mm::gainToDb(1.0f + maxClip) : 0.0f, std::memory_order_relaxed);
    reportGainReduction(clipDb.load(std::memory_order_relaxed));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SoftClipperProcessor(); }
