#include "plugins/reverb/ReverbProcessor.h"
#include "Params.h"
#include "plugins/reverb/ReverbEditor.h"
#include "reverb_data.h"
#include <cmath>
#include <numbers>

namespace rp = mm::plugins::reverb;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &reverb_data::getNamedResource;
    f.names = reverb_data::namedResourceList;
    f.count = reverb_data::namedResourceListSize;
    return f;
}
mm::reverb::FdnAlgorithm fdnAlgo(int a) noexcept
{
    return a == 0 ? mm::reverb::FdnAlgorithm::Room : (a == 3 ? mm::reverb::FdnAlgorithm::Chamber : mm::reverb::FdnAlgorithm::Hall);
}
mm::reverb::ErPattern erPattern(int a) noexcept
{
    return a == 0 ? mm::reverb::ErPattern::Room : (a == 3 ? mm::reverb::ErPattern::Chamber : mm::reverb::ErPattern::Hall);
}
} // namespace

ReverbProcessor::ReverbProcessor()
    : mm::PluginProcessorBase(rp::table(), rp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), rp::kPluginName)
{
    algorithm = paramRef(rp::id::algorithm);
    predelay = paramRef(rp::id::predelay_ms);
    size = paramRef(rp::id::size_pct);
    decay = &smoothed(rp::id::decay_s);
    damping = &smoothed(rp::id::damping_pct);
    diffusion = &smoothed(rp::id::diffusion_pct);
    lowCut = &smoothed(rp::id::low_cut_hz);
    highCut = &smoothed(rp::id::high_cut_hz);
    earlyLate = &smoothed(rp::id::early_late_pct);
    mix = &smoothed(rp::id::mix_pct);
    width = &smoothed(rp::id::width_pct);
}

// below RT60 0.5 s the room shrinks with the decay: short lines mix fast enough for the label to be met
static double effectiveSize(double sz, double rt60) noexcept { return sz * juce::jlimit(0.35, 1.0, rt60 / 0.5); }

void ReverbProcessor::configureFdn(mm::reverb::FDN<float>& f, int algo, double sz)
{
    f.setAlgorithm(fdnAlgo(algo), effectiveSize(sz, decay->current()));
    f.setRT60(decay->current());
    f.setDamping(damping->current() * 0.01f);
    f.setDiffusion(diffusion->current() * 0.01f);
}

void ReverbProcessor::prepareDsp(double sampleRate, int, int)
{
    sr = sampleRate;
    for (auto& f : fdn)
        f.prepare(sr, 0.15);
    plate.prepare(sr);
    early.prepare(sr);
    predelayLine.prepare(static_cast<int>(std::ceil(kMaxPredelayMs * 1e-3 * sr)) + 1, 2);
    for (auto& f : lowCutF)
    {
        f.prepare(sr);
        f.setMode(mm::SvfMode::HighPass);
    }
    for (auto& f : highCutF)
    {
        f.prepare(sr);
        f.setMode(mm::SvfMode::LowPass);
    }
    fadeStep = 1.0 / juce::jmax(1.0, kFadeMs * 1e-3 * sr);
    preFadeStep = 1.0 / juce::jmax(1.0, 0.02 * sr);
    resetDsp();
    const int algo = juce::jlimit(0, 3, static_cast<int>(std::lround(algorithm.get())));
    const double sz = size.get() * 0.01;
    cur = algo == 2 ? Plate : FdnA;
    prev = -1;
    fadePos = 1.0;
    configureFdn(fdn[0], algo, sz);
    configureFdn(fdn[1], algo, sz);
    plate.setRT60(decay->current());
    plate.setDamping(damping->current() * 0.01f);
    plate.setDiffusion(diffusion->current() * 0.01f);
    early.setPattern(erPattern(algo), sz);
    lastAlgorithm = algo;
    pendingAlgorithm = -1;
    lastSize = sz;
    preCur = preOld = juce::jlimit(0, static_cast<int>(kMaxPredelayMs * 1e-3 * sr), static_cast<int>(std::lround(predelay.get() * 1e-3 * sr)));
    preFade = 1.0;
}

void ReverbProcessor::resetDsp()
{
    for (auto& f : fdn)
        f.reset();
    plate.reset();
    early.reset();
    predelayLine.reset();
    for (auto& f : lowCutF)
        f.reset();
    for (auto& f : highCutF)
        f.reset();
}

// Algorithm change: configure the idle engine and crossfade to it (both run during the fade only). While a fade is
// running the request is queued (resetting the outgoing engine would cut the tail) and applied when the fade ends.
void ReverbProcessor::switchTo(int algo)
{
    if (fadePos < 1.0)
    {
        pendingAlgorithm = algo;
        return;
    }
    const double sz = lastSize;
    int next = algo == 2 ? Plate : (cur == FdnA ? FdnB : FdnA);
    if (cur == Plate && algo != 2)
        next = FdnA;
    if (next == Plate)
        plate.reset();
    else
    {
        configureFdn(fdn[static_cast<size_t>(next)], algo, sz);
        fdn[static_cast<size_t>(next)].reset();
    }
    if (next != cur)
    {
        prev = cur;
        cur = next;
        fadePos = 0.0;
    }
    early.setPattern(erPattern(algo), sz);
    pendingAlgorithm = -1;
}

void ReverbProcessor::engineProcess(int engine, float inL, float inR, float& outL, float& outR) noexcept
{
    if (engine == Plate)
        plate.processStereo(inL, inR, outL, outR);
    else
        fdn[static_cast<size_t>(engine)].processStereo(inL, inR, outL, outR);
}

void ReverbProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numIn = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels());
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), 2);
    if (numIn <= 0 || numOut <= 0)
        return;
    const float mixStart = mix->current() * 0.01f, elStart = earlyLate->current() * 0.01f, wStart = width->current() * 0.01f;
    for (auto* s : {decay, damping, diffusion, lowCut, highCut, earlyLate, mix, width})
        s->skip(n);
    const int algo = juce::jlimit(0, 3, static_cast<int>(std::lround(algorithm.get())));
    const double sz = size.get() * 0.01;
    const double effSize = effectiveSize(sz, decay->current());
    if (std::abs(sz - lastSize) > 0.005 || std::abs(effSize - lastEffectiveSize) > 0.005) // lengths glide; the plate ignores size
    {
        lastSize = sz;
        lastEffectiveSize = effSize;
        for (auto& f : fdn)
            f.setSize(effSize);
        early.setPattern(erPattern(lastAlgorithm), sz);
    }
    if (pendingAlgorithm >= 0 && fadePos >= 1.0)
        switchTo(pendingAlgorithm);
    if (algo != lastAlgorithm)
    {
        lastAlgorithm = algo;
        switchTo(algo);
    }
    const int pre = juce::jlimit(0, static_cast<int>(kMaxPredelayMs * 1e-3 * sr), static_cast<int>(std::lround(predelay.get() * 1e-3 * sr)));
    if (pre != preCur && preFade >= 1.0) // pre-delay: crossfade read positions over 20 ms (one move at a time)
    {
        preOld = preCur;
        preCur = pre;
        preFade = 0.0;
    }
    // per-block engine settings (smoothed values at the block end)
    for (auto& f : fdn)
    {
        f.setRT60(decay->current());
        f.setDamping(damping->current() * 0.01f);
        f.setDiffusion(diffusion->current() * 0.01f);
    }
    plate.setRT60(decay->current());
    plate.setDamping(damping->current() * 0.01f);
    plate.setDiffusion(diffusion->current() * 0.01f);
    const bool lcOn = lowCut->current() > 20.5f, hcOn = highCut->current() < 19999.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        lowCutF[static_cast<size_t>(ch)].setParams(lowCut->current(), 0.7071, 0.0);
        highCutF[static_cast<size_t>(ch)].setParams(highCut->current(), 0.7071, 0.0);
    }
    const float mixEnd = mix->current() * 0.01f, elEnd = earlyLate->current() * 0.01f, wEnd = width->current() * 0.01f;

    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float>(i + 1) / static_cast<float>(n);
        const float mixAmt = mixStart + (mixEnd - mixStart) * t;
        const float el = elStart + (elEnd - elStart) * t;
        const float w = wStart + (wEnd - wStart) * t;
        const float dryL = buffer.getSample(0, i);
        const float dryR = numIn > 1 ? buffer.getSample(1, i) : dryL;
        predelayLine.write(dryL, 0);
        predelayLine.write(dryR, 1);
        float inL = predelayLine.read(preCur, 0);
        float inR = predelayLine.read(preCur, 1);
        if (preFade < 1.0)
        {
            const float a = static_cast<float>(preFade * std::numbers::pi * 0.5);
            inL = predelayLine.read(preOld, 0) * std::cos(a) + inL * std::sin(a);
            inR = predelayLine.read(preOld, 1) * std::cos(a) + inR * std::sin(a);
            preFade = juce::jmin(1.0, preFade + preFadeStep);
        }
        float eL, eR, lL, lR;
        early.processStereo(inL, inR, eL, eR);
        engineProcess(cur, inL, inR, lL, lR);
        if (fadePos < 1.0 && prev >= 0)
        {
            float pL, pR;
            engineProcess(prev, inL, inR, pL, pR);
            const float a = static_cast<float>(fadePos * std::numbers::pi * 0.5);
            lL = pL * std::cos(a) + lL * std::sin(a);
            lR = pR * std::cos(a) + lR * std::sin(a);
            fadePos += fadeStep;
            if (fadePos >= 1.0)
                prev = -1;
        }
        float wetL = eL * el + lL * (1.0f - el);
        float wetR = eR * el + lR * (1.0f - el);
        if (lcOn)
        {
            wetL = lowCutF[0].process(wetL);
            wetR = lowCutF[1].process(wetR);
        }
        if (hcOn)
        {
            wetL = highCutF[0].process(wetL);
            wetR = highCutF[1].process(wetR);
        }
        const float m = 0.5f * (wetL + wetR), s = 0.5f * (wetL - wetR) * w;
        if (numOut > 1)
        {
            buffer.setSample(0, i, dryL * (1.0f - mixAmt) + (m + s) * mixAmt);
            buffer.setSample(1, i, dryR * (1.0f - mixAmt) + (m - s) * mixAmt);
        }
        else
            buffer.setSample(0, i, dryL * (1.0f - mixAmt) + m * mixAmt); // mono bus: the mono sum, independent of width
    }
}

juce::AudioProcessorEditor* ReverbProcessor::createEditor() { return new ReverbEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverbProcessor(); }
