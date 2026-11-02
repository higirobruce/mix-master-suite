#include "plugins/deesser/DeEsserProcessor.h"
#include "Params.h"
#include "core/common/Math.h"
#include "deesser_data.h"
#include "plugins/deesser/DeEsserEditor.h"
#include <cmath>

namespace dp = mm::plugins::deesser;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &deesser_data::getNamedResource;
    f.names = deesser_data::namedResourceList;
    f.count = deesser_data::namedResourceListSize;
    return f;
}
// Q of a band `octaves` wide: Q = 1 / (2 sinh(ln2 / 2 * octaves))
inline double qForOctaves(double octaves) noexcept { return 1.0 / (2.0 * std::sinh(0.5 * std::log(2.0) * std::max(0.05, octaves))); }
} // namespace

DeEsserProcessor::DeEsserProcessor()
    : mm::PluginProcessorBase(dp::table(), dp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), dp::kPluginName)
{
    mode = paramRef(dp::id::mode);
    listen = paramRef(dp::id::listen);
    lookaheadRaw = paramRef(dp::id::lookahead_ms); // unsmoothed integer latency (ADR-0009)
    freq = &smoothed(dp::id::freq_hz);
    threshold = &smoothed(dp::id::threshold_db);
    range = &smoothed(dp::id::range_db);
    width = &smoothed(dp::id::width_oct);
}

void DeEsserProcessor::prepareDsp(double sampleRate, int maxBlock, int)
{
    sr = sampleRate;
    designedFc = designedQ = -1.0; // the filters' coefficients depend on the rate
    maxLookaheadSamples = static_cast<int>(std::lround(kMaxLookaheadMs * 1e-3 * sr));
    lookahead.prepare(sr, kMaxLookaheadMs, kChannels);
    lookahead.setMs(lookaheadRaw.get());
    oldWindow = lookahead.latencySamples();
    tapFade = 1.0;
    tapFadeStep = 1.0 / juce::jmax(1.0, 0.005 * sr);
    for (int ch = 0; ch < kChannels; ++ch)
    {
        detectorBp[static_cast<size_t>(ch)].prepare(sr);
        detectorBp[static_cast<size_t>(ch)].setMode(mm::SvfMode::BandPass);
        bell[static_cast<size_t>(ch)].prepare(sr);
        bell[static_cast<size_t>(ch)].setMode(mm::SvfMode::Peak);
        for (auto* sp : {&splitDet[static_cast<size_t>(ch)], &splitOut[static_cast<size_t>(ch)]})
        {
            sp->prepare(sr);
            sp->setOrder(mm::LrOrder::LR4);
        }
    }
    env.prepare(sr);
    env.setAttackMs(kAttackMs);
    env.setReleaseMs(kReleaseMs);
    for (auto& d : dryBuf)
        d.assign(static_cast<size_t>(maxBlock), 0.0f);
    resetDsp();
}

void DeEsserProcessor::resetDsp()
{
    for (auto& f : detectorBp) f.reset();
    for (auto& f : bell) f.reset();
    for (auto& f : splitDet) f.reset();
    for (auto& f : splitOut) f.reset();
    lookahead.reset();
    env.reset();
    env.snapTo(0.0f);
    sibilant.store(false, std::memory_order_relaxed);
    reportGainReduction(0.0f);
}

void DeEsserProcessor::beginBlock(int)
{
    const int want = juce::jlimit(0, lookahead.maxWindowSamples(), static_cast<int>(std::lround(lookaheadRaw.get() * 1e-3 * sr)));
    if (want != lookahead.latencySamples())
    {
        oldWindow = tapFade >= 1.0 ? lookahead.latencySamples() : oldWindow;
        lookahead.setSamples(want);
        tapFade = 0.0;
    }
}

void DeEsserProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), kChannels);
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), kChannels);
    if (numMain <= 0 || numOut <= 0)
        return;
    for (auto* s : {freq, threshold, range, width})
        s->skip(n);
    const bool splitMode = mode.on();
    const bool lis = listen.on();
    const double fc = juce::jlimit(20.0, 0.45 * sr, static_cast<double>(freq->current()));
    const double q = qForOctaves(width->current());
    const float bpGain = static_cast<float>(1.0 / q); // the SVF band output peaks at Q: unity at fc
    const float thr = threshold->current(), rng = range->current();
    if (fc != designedFc || q != designedQ) // the designs cost trig: only when the band moved
    {
        for (int ch = 0; ch < kChannels; ++ch)
        {
            detectorBp[static_cast<size_t>(ch)].setParams(fc, q, 0.0);
            splitDet[static_cast<size_t>(ch)].setCrossover(fc);
            splitOut[static_cast<size_t>(ch)].setCrossover(fc);
        }
        designedFc = fc;
        designedQ = q;
    }
    const int window = lookahead.latencySamples();
    const bool passThrough = window == 0 && tapFade >= 1.0;

    // dry copy aligned to the lookahead (two taps while a lookahead change fades)
    for (int ch = 0; ch < numMain; ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        float* d = dryBuf[static_cast<size_t>(ch)].data();
        for (int i = 0; i < n; ++i)
        {
            lookahead.writeSignal(src[i], ch);
            float delayed = passThrough ? src[i] : lookahead.readDelayed(window, ch);
            if (tapFade < 1.0)
                delayed = static_cast<float>(lookahead.readDelayed(oldWindow, ch) * (1.0 - tapFade) + delayed * tapFade);
            d[i] = delayed;
            if (ch == numMain - 1 && tapFade < 1.0)
                tapFade = juce::jmin(1.0, tapFade + tapFadeStep);
        }
    }

    float maxReduction = 0.0f;
    bool anySibilant = false;
    float lowBand[kChannels] = {0.0f, 0.0f}, highBand[kChannels] = {0.0f, 0.0f};
    for (int i = 0; i < n; ++i)
    {
        // detector on the undelayed input (that is the lookahead), linked over channels
        float peak = 0.0f;
        for (int ch = 0; ch < numMain; ++ch)
        {
            const float x = buffer.getSample(ch, i);
            float band;
            if (splitMode)
            {
                splitDet[static_cast<size_t>(ch)].process(x, lowBand[ch], highBand[ch]);
                band = highBand[ch];
            }
            else
                band = detectorBp[static_cast<size_t>(ch)].process(x) * bpGain;
            peak = juce::jmax(peak, std::abs(band));
        }
        const float levelDb = mm::fastGainToDb(lookahead.maxEnvelope(peak));
        const float target = juce::jlimit(0.0f, rng, levelDb - thr);
        const float red = env.process(target);
        maxReduction = juce::jmax(maxReduction, red);
        anySibilant = anySibilant || target > 0.5f;
        if ((modCounter++ & 7) == 0 && !splitMode)
        {
            // the bell follows the reduction every 8 samples on a running counter (block-size independent; the SVF
            // is modulation-safe). The TPT peak's bandwidth scales with the cut depth (k = 1 / (Q A)): pass Q / A so
            // the bell keeps its labelled width at every reduction.
            const double a = mm::fastDbToGain(-0.5f * red); // 10^(-red/40)
            for (int ch = 0; ch < numMain; ++ch)
                bell[static_cast<size_t>(ch)].setParams(fc, q / a, -red);
        }
        const float hiGain = mm::fastDbToGain(-red);
        for (int ch = 0; ch < numOut; ++ch)
        {
            const int src = juce::jmin(ch, numMain - 1);
            const float dry = dryBuf[static_cast<size_t>(src)][static_cast<size_t>(i)];
            float wet;
            if (splitMode)
            {
                float lo, hi;
                splitOut[static_cast<size_t>(src)].process(dry, lo, hi); // its own state: the detector split saw the undelayed input
                wet = lo + hi * hiGain;
            }
            else
                wet = bell[static_cast<size_t>(src)].process(dry);
            buffer.setSample(ch, i, lis ? dry - wet : wet);
        }
    }
    sibilant.store(anySibilant, std::memory_order_relaxed);
    reportGainReduction(maxReduction);
}

juce::AudioProcessorEditor* DeEsserProcessor::createEditor() { return new DeEsserEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DeEsserProcessor(); }
