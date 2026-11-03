#include "plugins/multiband_comp/MultibandCompProcessor.h"
#include "Params.h"
#include "core/common/Math.h"
#include "core/filters/BiquadDesign.h"
#include "multiband_comp_data.h"
#include "plugins/multiband_comp/MultibandCompEditor.h"
#include <algorithm>
#include <cmath>

namespace mp = mm::plugins::multiband_comp;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &multiband_comp_data::getNamedResource;
    f.names = multiband_comp_data::namedResourceList;
    f.count = multiband_comp_data::namedResourceListSize;
    return f;
}
constexpr const char* kBandIds[MultibandCompProcessor::kMaxBands][7] = {
    {mp::id::b1_threshold_db, mp::id::b1_ratio, mp::id::b1_attack_ms, mp::id::b1_release_ms, mp::id::b1_gain_db, mp::id::b1_solo, mp::id::b1_bypass},
    {mp::id::b2_threshold_db, mp::id::b2_ratio, mp::id::b2_attack_ms, mp::id::b2_release_ms, mp::id::b2_gain_db, mp::id::b2_solo, mp::id::b2_bypass},
    {mp::id::b3_threshold_db, mp::id::b3_ratio, mp::id::b3_attack_ms, mp::id::b3_release_ms, mp::id::b3_gain_db, mp::id::b3_solo, mp::id::b3_bypass},
    {mp::id::b4_threshold_db, mp::id::b4_ratio, mp::id::b4_attack_ms, mp::id::b4_release_ms, mp::id::b4_gain_db, mp::id::b4_solo, mp::id::b4_bypass},
    {mp::id::b5_threshold_db, mp::id::b5_ratio, mp::id::b5_attack_ms, mp::id::b5_release_ms, mp::id::b5_gain_db, mp::id::b5_solo, mp::id::b5_bypass},
};
constexpr const char* kXoverIds[MultibandCompProcessor::kMaxCrossovers] = {mp::id::xover_1_hz, mp::id::xover_2_hz, mp::id::xover_3_hz, mp::id::xover_4_hz};
constexpr double kLrQ = 0.70710678118654752; // an LR4's low + high is the 2nd-order all-pass at this Q (ADR-0010)
} // namespace

MultibandCompProcessor::MultibandCompProcessor()
    : mm::PluginProcessorBase(mp::table(), mp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                                  .withInput("Sidechain", juce::AudioChannelSet::stereo(), false),
                              factory(), mp::kPluginName)
{
    for (int b = 0; b < kMaxBands; ++b)
    {
        auto& r = bands[static_cast<size_t>(b)];
        r.threshold = &smoothed(kBandIds[b][0]);
        r.ratio = &smoothed(kBandIds[b][1]);
        r.attack = &smoothed(kBandIds[b][2]);
        r.release = &smoothed(kBandIds[b][3]);
        r.gain = &smoothed(kBandIds[b][4]);
        r.solo = paramRef(kBandIds[b][5]);
        r.bypass = paramRef(kBandIds[b][6]);
    }
    for (int i = 0; i < kMaxCrossovers; ++i)
        xover[static_cast<size_t>(i)] = &smoothed(kXoverIds[i]);
    numBandsRef = paramRef(mp::id::num_bands);
    detectorRef = paramRef(mp::id::detector);
    scExternal = paramRef(mp::id::sc_external);
    mix = &smoothed(mp::id::mix_pct);
    for (auto& x : xoverInForce)
        x.store(0.0);
}

juce::AudioProcessorEditor* MultibandCompProcessor::createEditor() { return new MultibandCompEditor(*this); }

bool MultibandCompProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return mm::PluginProcessorBase::isBusesLayoutSupported(layouts);
}

int MultibandCompProcessor::numBands() const noexcept { return 3 + juce::jlimit(0, 2, static_cast<int>(std::lround(numBandsRef.get()))); }

void MultibandCompProcessor::Tree::reset() noexcept
{
    for (auto& s : split)
        s.reset();
    for (auto& band : ap)
        for (auto& f : band)
            f.reset();
}

void MultibandCompProcessor::prepareDsp(double sampleRate, int maxBlock, int)
{
    sr = sampleRate;
    switchStep = 1.0 / std::max(1.0, kSwitchMs * 1e-3 * sr);
    fadeStep = switchStep;
    warmSamples = static_cast<int>(kWarmMs * 1e-3 * sr);
    for (auto* t : {&trees, &keyTrees, &fadeTrees})
        for (auto& tree : *t)
            for (auto& s : tree.split)
            {
                s.prepare(sr);
                s.setOrder(mm::LrOrder::LR4);
            }
    for (auto& b : bands)
    {
        b.detector.prepare(sr);
        b.detector.setRmsWindowMs(kRmsMs);
        b.env.prepare(sr);
    }
    analyser.prepare(sr, 12, kChannels);
    designedHz.fill(-1.0);
    snapSmoothersToTargets();
    designCrossovers(true);
    resetDsp();
    juce::ignoreUnused(maxBlock);
}

void MultibandCompProcessor::resetDsp()
{
    for (auto& t : trees)
        t.reset();
    for (auto& t : keyTrees)
        t.reset();
    const int n = numBands();
    bool anySolo = false;
    for (int b = 0; b < n; ++b)
        anySolo = anySolo || bands[static_cast<size_t>(b)].solo.on();
    for (int b = 0; b < kMaxBands; ++b)
    {
        auto& r = bands[static_cast<size_t>(b)];
        r.detector.reset();
        r.env.reset();
        r.env.snapTo(0.0f);
        r.activePos = r.activeTarget = r.bypass.on() ? 0.0 : 1.0;
        r.audiblePos = r.audibleTarget = (!anySolo || r.solo.on()) ? 1.0 : 0.0;
        reduction[static_cast<size_t>(b)].store(0.0f);
        levelDb[static_cast<size_t>(b)].store(-120.0f);
    }
    lastNumBands = n;
    fadeN = 0;
}

// crossovers from the smoothed parameters, each held a third of an octave above the one below; the all-pass legs
// of every tree share the coefficients of the crossover they stand in for
void MultibandCompProcessor::designCrossovers(bool force) noexcept
{
    const int n = numBands();
    double prev = 0.0;
    for (int i = 0; i < kMaxCrossovers; ++i)
    {
        double f = static_cast<double>(xover[static_cast<size_t>(i)]->current());
        if (i > 0)
            f = std::max(f, prev * std::pow(2.0, kMinRatioOct));
        f = juce::jlimit(10.0, 0.45 * sr, f);
        prev = f;
        xoverInForce[static_cast<size_t>(i)].store(i < n - 1 ? f : 0.0, std::memory_order_relaxed);
        if (!force && f == designedHz[static_cast<size_t>(i)])
            continue;
        designedHz[static_cast<size_t>(i)] = f;
        apCoeffs[static_cast<size_t>(i)] = mm::rbj::allPass(sr, f, kLrQ);
        for (auto* t : {&trees, &keyTrees})
            for (auto& tree : *t)
            {
                tree.split[static_cast<size_t>(i)].setCrossover(f);
                for (auto& band : tree.ap)
                    band[static_cast<size_t>(i)].setCoeffs(apCoeffs[static_cast<size_t>(i)]);
            }
    }
}

void MultibandCompProcessor::splitBands(Tree& t, double x, double* out, int n) noexcept
{
    double rest = x;
    for (int k = 0; k < n - 1; ++k)
    {
        double lo = 0.0, hi = 0.0;
        t.split[static_cast<size_t>(k)].process(rest, lo, hi);
        out[k] = lo;
        rest = hi;
    }
    out[n - 1] = rest;
    // band k passes through the all-passes of crossovers k + 1 .. n - 2 so every band carries the same phase
    for (int k = 0; k < n - 1; ++k)
        for (int c = k + 1; c < n - 1; ++c)
            out[k] = t.ap[static_cast<size_t>(k)][static_cast<size_t>(c)].process(out[k]);
}

void MultibandCompProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), kChannels);
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), kChannels);
    if (numMain <= 0 || numOut <= 0)
        return;
    const int n = numBands();
    if (n != lastNumBands)
    {
        // the outgoing band count keeps running on a copy of its tree while the new tree starts clean, warms up
        // silently for kWarmMs and then crossfades in over kSwitchMs (an instant switch stepped the output 6-7x
        // its own slope: review finding). The copy is of fixed-size filter state: nothing allocates.
        fadeN = lastNumBands;
        fadeTrees = trees;
        fadeWarm = warmSamples;
        fadePos = 0.0;
        lastNumBands = n;
        for (auto& t : trees)
            t.reset();
        for (auto& t : keyTrees)
            t.reset();
    }
    for (auto* s : xover)
        s->skip(numSamples);
    mix->skip(numSamples);
    const bool rms = detectorRef.get() >= 0.5f;
    bool anySolo = false;
    for (int b = 0; b < n; ++b)
        anySolo = anySolo || bands[static_cast<size_t>(b)].solo.on();
    for (int b = 0; b < kMaxBands; ++b)
    {
        auto& r = bands[static_cast<size_t>(b)];
        for (auto* s : {r.threshold, r.ratio, r.attack, r.release, r.gain})
            s->skip(numSamples);
        mm::GainComputerParams gp;
        gp.thresholdDb = r.threshold->current();
        gp.ratio = r.ratio->current();
        gp.kneeDb = kKneeDb;
        r.gc.set(gp);
        r.detector.setMode(rms ? mm::DetectorMode::Rms : mm::DetectorMode::Peak);
        r.env.setAttackMs(r.attack->current());
        r.env.setReleaseMs(r.release->current());
        r.activeTarget = r.bypass.on() ? 0.0 : 1.0;
        r.audibleTarget = (b < n && (!anySolo || r.solo.on())) ? 1.0 : 0.0;
    }
    // external key
    const float* key[kChannels] = {nullptr, nullptr};
    int keyChannels = 0;
    const auto* scBus = getBus(true, 1);
    if (scExternal.on() && scBus != nullptr && scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
    {
        auto scBuffer = getBusBuffer(buffer, true, 1);
        keyChannels = juce::jmin(scBuffer.getNumChannels(), kChannels);
        for (int ch = 0; ch < keyChannels; ++ch)
            key[ch] = scBuffer.getReadPointer(ch);
    }
    float* out[kChannels] = {buffer.getWritePointer(0), numOut > 1 ? buffer.getWritePointer(1) : nullptr};
    const float* in[kChannels] = {buffer.getReadPointer(0), numMain > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0)};
    const float mixAmt = mix->current() * 0.01f;
    float maxRed[kMaxBands] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, maxLvl[kMaxBands] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    double bandSig[kChannels][kMaxBands], keySig[kChannels][kMaxBands], oldSig[kChannels][kMaxBands], gains[kMaxBands];
    for (int i = 0; i < numSamples; ++i)
    {
        if ((modCounter++ & 31) == 0)
            designCrossovers(false);
        for (int ch = 0; ch < numMain; ++ch)
        {
            splitBands(trees[static_cast<size_t>(ch)], in[ch][i], bandSig[ch], n);
            if (fadeN > 0)
                splitBands(fadeTrees[static_cast<size_t>(ch)], in[ch][i], oldSig[ch], fadeN);
            if (keyChannels > 0)
                splitBands(keyTrees[static_cast<size_t>(ch)], key[juce::jmin(ch, keyChannels - 1)][i], keySig[ch], n);
        }
        double wet[kChannels] = {0.0, 0.0}, dry[kChannels] = {0.0, 0.0};
        for (int b = 0; b < n; ++b)
        {
            auto& r = bands[static_cast<size_t>(b)];
            float lvl = 0.0f;
            for (int ch = 0; ch < numMain; ++ch)
                lvl = std::max(lvl, static_cast<float>(std::abs(keyChannels > 0 ? keySig[ch][b] : bandSig[ch][b])));
            const float det = r.detector.process(lvl);
            const float lvlDb = mm::fastGainToDb(det);
            const float red = r.env.process(-r.gc.gainDb(lvlDb));
            maxRed[b] = std::max(maxRed[b], red);
            maxLvl[b] = std::max(maxLvl[b], det);
            if (r.activePos != r.activeTarget)
                r.activePos = r.activePos < r.activeTarget ? std::min(r.activeTarget, r.activePos + switchStep) : std::max(r.activeTarget, r.activePos - switchStep);
            if (r.audiblePos != r.audibleTarget)
                r.audiblePos = r.audiblePos < r.audibleTarget ? std::min(r.audibleTarget, r.audiblePos + switchStep) : std::max(r.audibleTarget, r.audiblePos - switchStep);
            const double compGain = red <= 0.0f ? 1.0 : static_cast<double>(mm::fastDbToGain(-red));
            const double gain = (1.0 + (compGain * static_cast<double>(mm::fastDbToGain(r.gain->current())) - 1.0) * r.activePos) * r.audiblePos;
            gains[b] = gain;
            for (int ch = 0; ch < numMain; ++ch)
            {
                dry[ch] += bandSig[ch][b];
                wet[ch] += bandSig[ch][b] * gain;
            }
        }
        if (fadeN > 0)
        {
            // the outgoing tree's bands take the nearest new band's gain; output = old during the warm-up, then a fade
            double wetOld[kChannels] = {0.0, 0.0}, dryOld[kChannels] = {0.0, 0.0};
            for (int b = 0; b < fadeN; ++b)
            {
                const double g = gains[std::min(b, n - 1)];
                for (int ch = 0; ch < numMain; ++ch)
                {
                    dryOld[ch] += oldSig[ch][b];
                    wetOld[ch] += oldSig[ch][b] * g;
                }
            }
            double t = 0.0;
            if (fadeWarm > 0)
                --fadeWarm;
            else
                t = fadePos = std::min(1.0, fadePos + fadeStep);
            for (int ch = 0; ch < numMain; ++ch)
            {
                dry[ch] = dryOld[ch] + (dry[ch] - dryOld[ch]) * t;
                wet[ch] = wetOld[ch] + (wet[ch] - wetOld[ch]) * t;
            }
            if (fadePos >= 1.0)
                fadeN = 0;
        }
        for (int ch = 0; ch < numOut; ++ch)
        {
            const int src = juce::jmin(ch, numMain - 1);
            out[ch][i] = static_cast<float>(dry[src] + (wet[src] - dry[src]) * mixAmt);
        }
    }
    for (int b = 0; b < kMaxBands; ++b)
    {
        reduction[static_cast<size_t>(b)].store(b < n ? maxRed[b] : 0.0f, std::memory_order_relaxed);
        levelDb[static_cast<size_t>(b)].store(b < n ? mm::fastGainToDb(maxLvl[b]) : -120.0f, std::memory_order_relaxed);
    }
    float overall = 0.0f;
    for (int b = 0; b < n; ++b)
        overall = std::max(overall, maxRed[b]);
    reportGainReduction(overall);
    analyser.push(buffer.getArrayOfReadPointers(), numOut, numSamples);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MultibandCompProcessor(); }
