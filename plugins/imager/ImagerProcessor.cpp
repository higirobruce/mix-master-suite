#include "plugins/imager/ImagerProcessor.h"
#include "Params.h"
#include "core/common/Math.h"
#include "core/filters/BiquadDesign.h"
#include "imager_data.h"
#include "plugins/imager/ImagerEditor.h"
#include <cmath>
#include <numbers>

namespace ip = mm::plugins::imager;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &imager_data::getNamedResource;
    f.names = imager_data::namedResourceList;
    f.count = imager_data::namedResourceListSize;
    return f;
}
} // namespace

ImagerProcessor::ImagerProcessor()
    : mm::PluginProcessorBase(ip::table(), ip::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), ip::kPluginName)
{
    xoverLow = &smoothed(ip::id::xover_low_hz);
    xoverHigh = &smoothed(ip::id::xover_high_hz);
    widthLow = &smoothed(ip::id::width_low_pct);
    widthMid = &smoothed(ip::id::width_mid_pct);
    widthHigh = &smoothed(ip::id::width_high_pct);
    pan = &smoothed(ip::id::pan);
    monoBelow = paramRef(ip::id::mono_below_hz);
    output = &smoothed(ip::id::output_db);
    scopeL.assign(kScopeSize, 0.0f);
    scopeR.assign(kScopeSize, 0.0f);
}

void ImagerProcessor::prepareDsp(double sampleRate, int, int)
{
    sr = sampleRate;
    for (auto* x : {&sLow, &sHigh, &sMono})
    {
        x->prepare(sr);
        x->setOrder(mm::LrOrder::LR4);
    }
    apLowHz = apHighHz = 0.0;
    correlation.prepare(sr, 300.0);
    monoMixStep = 1.0 / (kMonoFadeMs * 1e-3 * sr);
    monoGlideOctPerSample = 1.0 / (kMonoGlideMsPerOctave * 1e-3 * sr);
    resetDsp();
}

void ImagerProcessor::resetDsp()
{
    for (auto* x : {&sLow, &sHigh, &sMono})
        x->reset();
    for (auto* b : {&sLowAp, &mAp1, &mAp2, &mMonoAp})
        b->reset();
    correlation.reset();
    monoActive = false;
    monoMix = 0.0;
}

int ImagerProcessor::pullScope(float* l, float* r, int maxSamples) const noexcept
{
    const int n = juce::jmin(maxSamples, kScopeSize);
    const int w = scopeWrite.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
    {
        const int idx = ((w - n + i) % kScopeSize + kScopeSize) % kScopeSize;
        l[i] = scopeL[static_cast<size_t>(idx)];
        r[i] = scopeR[static_cast<size_t>(idx)];
    }
    return n;
}

void ImagerProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numIn = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels());
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), 2);
    if (numIn <= 0 || numOut <= 0)
        return;
    const float wStart[3] = {widthLow->current() * 0.01f, widthMid->current() * 0.01f, widthHigh->current() * 0.01f};
    const float panStart = pan->current() * 0.01f, outStartDb = output->current();
    for (auto* s : {xoverLow, xoverHigh, widthLow, widthMid, widthHigh, pan, output})
        s->skip(n);
    const float wEnd[3] = {widthLow->current() * 0.01f, widthMid->current() * 0.01f, widthHigh->current() * 0.01f};
    const float panEnd = pan->current() * 0.01f;
    const float oStart = mm::dbToGain(outStartDb), oEnd = mm::dbToGain(output->current());
    // constant-power pan (centre unity, full pan +3 dB on the kept side) times output: gains ramp over the block
    auto panGains = [](float p, float gain, float& l, float& r) {
        const float theta = (p + 1.0f) * 0.25f * std::numbers::pi_v<float>;
        l = 1.41421356f * std::cos(theta) * gain;
        r = 1.41421356f * std::sin(theta) * gain;
    };
    float gLStart, gRStart, gLEnd, gREnd;
    panGains(panStart, oStart, gLStart, gRStart);
    panGains(panEnd, oEnd, gLEnd, gREnd);
    const double fLow = xoverLow->current(), fHigh = juce::jmax(static_cast<double>(xoverHigh->current()), fLow * 1.5);
    constexpr double kLrQ = 0.70710678118654752;
    if (fLow != apLowHz)
    {
        sLow.setCrossover(fLow);
        mAp1.setCoeffs(mm::rbj::allPass(sr, fLow, kLrQ));
        apLowHz = fLow;
    }
    if (fHigh != apHighHz)
    {
        sHigh.setCrossover(fHigh);
        const auto ap = mm::rbj::allPass(sr, fHigh, kLrQ);
        sLowAp.setCoeffs(ap);
        mAp2.setCoeffs(ap);
        apHighHz = fHigh;
    }
    // mono-below: on/off is a 20 ms crossfade (the filters start at the target cutoff), moves while on glide at
    // 5 ms per octave, updated per block
    const double mbTarget = monoBelow.get();
    const bool want = mbTarget >= kMonoMinHz;
    if (want && !monoActive)
    {
        monoActive = true;
        monoFc = mbTarget;
        sMono.reset();
        mMonoAp.reset();
        sMono.setCrossover(monoFc);
        mMonoAp.setCoeffs(mm::rbj::allPass(sr, monoFc, kLrQ));
    }
    else if (want && monoFc != mbTarget)
    {
        const double maxOct = monoGlideOctPerSample * n, want2 = std::log2(mbTarget / monoFc);
        monoFc *= std::exp2(juce::jlimit(-maxOct, maxOct, want2));
        sMono.setCrossover(monoFc);
        mMonoAp.setCoeffs(mm::rbj::allPass(sr, monoFc, kLrQ));
    }
    const double mixTarget = want ? 1.0 : 0.0;
    int w = scopeWrite.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float>(i + 1) / static_cast<float>(n);
        const float inL = buffer.getSample(0, i);
        const float inR = numIn > 1 ? buffer.getSample(1, i) : inL;
        const double m = 0.5 * (inL + inR), s = 0.5 * (inL - inR);
        // side: three bands scaled by width; mid: the same networks (all-pass) so the phase matches
        double sl, sr1, sm, sh;
        sLow.process(s, sl, sr1);
        sHigh.process(sr1, sm, sh);
        const float g0 = wStart[0] + (wEnd[0] - wStart[0]) * t, g1 = wStart[1] + (wEnd[1] - wStart[1]) * t, g2 = wStart[2] + (wEnd[2] - wStart[2]) * t;
        double sOut = sLowAp.process(sl) * g0 + sm * g1 + sh * g2;
        double mOut = mAp2.process(mAp1.process(m));
        if (monoActive)
        {
            double lo, hi;
            sMono.process(sOut, lo, hi);
            const double mAp = mMonoAp.process(mOut);
            monoMix += juce::jlimit(-monoMixStep, monoMixStep, mixTarget - monoMix);
            sOut += (hi - sOut) * monoMix;  // no side below the mono frequency
            mOut += (mAp - mOut) * monoMix; // the matching all-pass on the mid
        }
        float outL = static_cast<float>(mOut + sOut), outR = static_cast<float>(mOut - sOut);
        outL *= gLStart + (gLEnd - gLStart) * t;
        outR *= gRStart + (gREnd - gRStart) * t;
        if (numOut > 1)
        {
            buffer.setSample(0, i, outL);
            buffer.setSample(1, i, outR);
        }
        else
            buffer.setSample(0, i, 0.5f * (outL + outR));
        scopeL[static_cast<size_t>(w)] = outL;
        scopeR[static_cast<size_t>(w)] = outR;
        w = (w + 1) % kScopeSize;
    }
    if (!want && monoMix <= 0.0)
        monoActive = false;
    scopeWrite.store(w, std::memory_order_release);
    if (numOut > 1)
    {
        correlation.process(buffer.getReadPointer(0), buffer.getReadPointer(1), n);
        correlationValue.store(correlation.value(), std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* ImagerProcessor::createEditor() { return new ImagerEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ImagerProcessor(); }
