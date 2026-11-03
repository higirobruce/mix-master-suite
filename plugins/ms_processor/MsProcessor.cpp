#include "plugins/ms_processor/MsProcessor.h"
#include "Params.h"
#include "core/common/Math.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/MatchedBiquad.h"
#include "core/filters/Transfer.h"
#include "ms_processor_data.h"
#include "plugins/ms_processor/MsEditor.h"
#include <algorithm>
#include <cmath>

namespace msp = mm::plugins::ms_processor;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &ms_processor_data::getNamedResource;
    f.names = ms_processor_data::namedResourceList;
    f.count = ms_processor_data::namedResourceListSize;
    return f;
}
constexpr const char* kBandIds[2][MsProcessor::kBands][5] = {
    {{msp::id::mid_eq1_enabled, msp::id::mid_eq1_type, msp::id::mid_eq1_freq_hz, msp::id::mid_eq1_gain_db, msp::id::mid_eq1_q},
     {msp::id::mid_eq2_enabled, msp::id::mid_eq2_type, msp::id::mid_eq2_freq_hz, msp::id::mid_eq2_gain_db, msp::id::mid_eq2_q}},
    {{msp::id::side_eq1_enabled, msp::id::side_eq1_type, msp::id::side_eq1_freq_hz, msp::id::side_eq1_gain_db, msp::id::side_eq1_q},
     {msp::id::side_eq2_enabled, msp::id::side_eq2_type, msp::id::side_eq2_freq_hz, msp::id::side_eq2_gain_db, msp::id::side_eq2_q}},
};
constexpr double kLrQ = 0.70710678118654752; // an LR4's low + high is the 2nd-order all-pass at this Q (ADR-0010)
} // namespace

MsProcessor::MsProcessor()
    : mm::PluginProcessorBase(msp::table(), msp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), msp::kPluginName)
{
    for (int lane = 0; lane < 2; ++lane)
        for (int b = 0; b < kBands; ++b)
        {
            auto& band = lanes[static_cast<size_t>(lane)][static_cast<size_t>(b)];
            band.enabled = paramRef(kBandIds[lane][b][0]);
            band.type = paramRef(kBandIds[lane][b][1]);
            band.freqRaw = paramRef(kBandIds[lane][b][2]);
            band.gainRaw = paramRef(kBandIds[lane][b][3]);
            band.qRaw = paramRef(kBandIds[lane][b][4]);
            band.freq = &smoothed(kBandIds[lane][b][2]);
            band.gain = &smoothed(kBandIds[lane][b][3]);
            band.q = &smoothed(kBandIds[lane][b][4]);
        }
    midDb = &smoothed(msp::id::mid_db);
    sideDb = &smoothed(msp::id::side_db);
    width = &smoothed(msp::id::width_pct);
    output = &smoothed(msp::id::output_db);
    monoBelow = paramRef(msp::id::mono_below_hz);
    soloRef = paramRef(msp::id::solo);
    scopeL.assign(kScopeSize, 0.0f);
    scopeR.assign(kScopeSize, 0.0f);
}

juce::AudioProcessorEditor* MsProcessor::createEditor() { return new MsEditor(*this); }

mm::BiquadCoeffs<double> MsProcessor::coeffsFor(int type, double rate, double freq, double gainDb, double q) noexcept
{
    const double f = juce::jlimit(1.0, 0.49 * rate, freq);
    switch (type)
    {
    case 1: return mm::matched::lowShelf(rate, f, gainDb, q);
    case 2: return mm::matched::highShelf(rate, f, gainDb, q);
    default: return mm::matched::peak(rate, f, gainDb, q);
    }
}

std::complex<double> MsProcessor::staticResponseAt(int lane, double hz) const
{
    const double rate = getSampleRate() > 0.0 ? getSampleRate() : sr;
    std::complex<double> h = 1.0;
    for (const auto& b : lanes[static_cast<size_t>(juce::jlimit(0, 1, lane))])
        if (b.enabled.on())
            h *= mm::evaluate(coeffsFor(static_cast<int>(std::lround(b.type.get())), rate, b.freqRaw.get(), b.gainRaw.get(), b.qRaw.get()), rate, hz);
    return h;
}

void MsProcessor::prepareDsp(double sampleRate, int, int channels)
{
    sr = sampleRate;
    analyserOut.prepare(sr, 11, juce::jmin(channels, 2));
    sMono.prepare(sr);
    sMono.setOrder(mm::LrOrder::LR4);
    correlation.prepare(sr, 300.0);
    monoMixStep = 1.0 / (kMonoFadeMs * 1e-3 * sr);
    monoGlideOctPerSample = 1.0 / (kMonoGlideMsPerOctave * 1e-3 * sr);
    switchStep = 1.0 / std::max(1.0, kSwitchMs * 1e-3 * sr);
    for (auto& lane : lanes)
        for (auto& b : lane)
            b.lastFreq = -1.0;
    snapSmoothersToTargets();
    resetDsp();
}

void MsProcessor::resetDsp()
{
    sMono.reset();
    mMonoAp.reset();
    correlation.reset();
    const double mb = monoBelow.get();
    monoActive = mb >= kMonoMinHz; // stays engaged across a reset (M4 review)
    monoMix = monoActive ? 1.0 : 0.0;
    if (monoActive)
    {
        monoFc = mb;
        sMono.setCrossover(monoFc);
        mMonoAp.setCoeffs(mm::rbj::allPass(sr, monoFc, kLrQ));
    }
    const int solo = static_cast<int>(std::lround(soloRef.get()));
    soloMid = solo == 2 ? 0.0 : 1.0;
    soloSide = solo == 1 ? 0.0 : 1.0;
    for (auto& lane : lanes)
        for (auto& b : lane)
        {
            b.filter.reset();
            b.onPos = b.onTarget = b.enabled.on() ? 1.0 : 0.0;
            designBand(b);
        }
}

void MsProcessor::designBand(Band& b) noexcept
{
    const double f = b.freq->current(), g = b.gain->current(), q = b.q->current();
    const int type = static_cast<int>(std::lround(b.type.get()));
    if (f == b.lastFreq && g == b.lastGain && q == b.lastQ && type == b.lastType)
        return;
    b.lastFreq = f;
    b.lastGain = g;
    b.lastQ = q;
    b.lastType = type;
    b.filter.setCoeffs(coeffsFor(type, sr, f, g, q));
}

int MsProcessor::pullScope(float* l, float* r, int maxSamples) const noexcept
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

void MsProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numIn = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels());
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), 2);
    if (numIn <= 0 || numOut <= 0)
        return;
    const float mStartDb = midDb->current(), sStartDb = sideDb->current(), wStart = width->current() * 0.01f, oStartDb = output->current();
    for (auto* s : {midDb, sideDb, width, output})
        s->skip(n);
    for (auto& lane : lanes)
        for (auto& b : lane)
        {
            for (auto* s : {b.freq, b.gain, b.q})
                s->skip(n);
            b.onTarget = b.enabled.on() ? 1.0 : 0.0;
        }
    const float mStart = mm::dbToGain(mStartDb), mEnd = mm::dbToGain(midDb->current());
    const float sStart = mm::dbToGain(sStartDb) * wStart, sEnd = mm::dbToGain(sideDb->current()) * width->current() * 0.01f;
    const float oStart = mm::dbToGain(oStartDb), oEnd = mm::dbToGain(output->current());
    const int solo = static_cast<int>(std::lround(soloRef.get()));
    const double soloMidTarget = solo == 2 ? 0.0 : 1.0, soloSideTarget = solo == 1 ? 0.0 : 1.0;
    // mono-below: engaged by a 20 ms crossfade with the filters at the target cutoff, glided while on (ADR-0010)
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
    const double mixTarget = want ? 1.0 : 0.0;
    const bool gliding = want && monoFc != mbTarget;
    int w = scopeWrite.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float>(i + 1) / static_cast<float>(n);
        const float inL = buffer.getSample(0, i);
        const float inR = numIn > 1 ? buffer.getSample(1, i) : inL;
        double m = 0.5 * (inL + inR), s = 0.5 * (inL - inR);
        m *= mStart + (mEnd - mStart) * t;
        s *= sStart + (sEnd - sStart) * t;
        if ((modCounter++ & 31) == 0)
            for (auto& lane : lanes)
                for (auto& b : lane)
                    designBand(b);
        double* laneSig[2] = {&m, &s};
        for (int lane = 0; lane < 2; ++lane)
            for (auto& b : lanes[static_cast<size_t>(lane)])
            {
                if (b.onPos != b.onTarget)
                    b.onPos = b.onPos < b.onTarget ? std::min(b.onTarget, b.onPos + switchStep) : std::max(b.onTarget, b.onPos - switchStep);
                if (b.onPos > 0.0)
                {
                    const double y = b.filter.process(*laneSig[lane]);
                    *laneSig[lane] += (y - *laneSig[lane]) * b.onPos;
                }
                else
                    b.filter.reset();
            }
        if (gliding && (i & 15) == 0)
        {
            const double maxOct = monoGlideOctPerSample * 16.0, want2 = std::log2(mbTarget / monoFc);
            monoFc *= std::exp2(juce::jlimit(-maxOct, maxOct, want2));
            sMono.setCrossover(monoFc);
            mMonoAp.setCoeffs(mm::rbj::allPass(sr, monoFc, kLrQ));
        }
        if (monoActive)
        {
            double lo, hi;
            sMono.process(s, lo, hi);
            const double mAp = mMonoAp.process(m);
            monoMix += juce::jlimit(-monoMixStep, monoMixStep, mixTarget - monoMix);
            s += (hi - s) * monoMix;
            m += (mAp - m) * monoMix;
        }
        soloMid += juce::jlimit(-switchStep, switchStep, soloMidTarget - soloMid);
        soloSide += juce::jlimit(-switchStep, switchStep, soloSideTarget - soloSide);
        m *= soloMid;
        s *= soloSide;
        const float og = oStart + (oEnd - oStart) * t;
        const float outL = static_cast<float>(m + s) * og, outR = static_cast<float>(m - s) * og;
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
    analyserOut.push(buffer.getArrayOfReadPointers(), numOut, n);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MsProcessor(); }
