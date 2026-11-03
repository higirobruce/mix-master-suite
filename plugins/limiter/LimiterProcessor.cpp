#include "plugins/limiter/LimiterProcessor.h"
#include "Params.h"
#include "core/common/Math.h"
#include "limiter_data.h"
#include "plugins/limiter/LimiterEditor.h"
#include <algorithm>
#include <cmath>

namespace lim = mm::plugins::limiter;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &limiter_data::getNamedResource;
    f.names = limiter_data::namedResourceList;
    f.count = limiter_data::namedResourceListSize;
    return f;
}
// one-pole coefficient for a 10-90 % time of `ms` at `rate`
inline double releaseCoeff(double ms, double rate) noexcept { return std::exp(-std::log(9.0) / std::max(1.0, ms * 1e-3 * rate)); }
} // namespace

LimiterProcessor::LimiterProcessor()
    : mm::PluginProcessorBase(lim::table(), lim::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), lim::kPluginName)
{
    input = &smoothed(lim::id::input_db);
    ceiling = &smoothed(lim::id::ceiling_dbtp);
    release = &smoothed(lim::id::release_ms);
    link = &smoothed(lim::id::link_pct);
    autoRelease = paramRef(lim::id::auto_release);
    lookaheadRaw = paramRef(lim::id::lookahead_ms);
    characterRef = paramRef(lim::id::character);
    ditherRef = paramRef(lim::id::dither);
}

juce::AudioProcessorEditor* LimiterProcessor::createEditor() { return new LimiterEditor(*this); }

void LimiterProcessor::prepareDsp(double sampleRate, int maxBlock, int channels)
{
    sr = sampleRate;
    factor = 4; // 4x at every rate (spec): 2x from 96 k, as BS.1770 Annex 2 allows for metering, let 0.85 dB of a loud noise's inter-sample peaks through
    os.prepare(sr, maxBlock, kChannels, factor);
    osRate = sr * factor;
    maxLookaheadHost = static_cast<int>(std::lround(kMaxLookaheadMs * 1e-3 * sr));
    minLookaheadHost = std::max(1, static_cast<int>(std::lround(kMinLookaheadMs * 1e-3 * sr)));
    maxWindowOs = std::max(1, maxLookaheadHost * factor);
    delay.prepare(osRate, kMaxLookaheadMs + 1.0, kChannels, maxBlock * factor);
    for (auto& c : chans)
    {
        c.hold.prepare(osRate, kMaxLookaheadMs + 1.0, 1);
        c.avgRing.assign(static_cast<size_t>(maxWindowOs + 1), 0.0f);
    }
    monoLane.assign(static_cast<size_t>(std::max(1, maxBlock)), 0.0f);
    tpMeter.prepare(sr, kChannels, maxBlock);
    lmMeter.prepare(sr, kChannels, 600.0);
    crestWindow = std::max(1, static_cast<int>(0.05 * osRate));
    tapFadeStep = 1.0 / std::max(1.0, 0.005 * osRate);
    for (int ch = 0; ch < kChannels; ++ch)
        dither[static_cast<size_t>(ch)].seed(0x1234567u * static_cast<unsigned>(ch + 1));
    snapSmoothersToTargets();
    lookaheadHost = juce::jlimit(minLookaheadHost, maxLookaheadHost, static_cast<int>(std::lround(lookaheadRaw.get() * 1e-3 * sr)));
    windowOs = oldWindowOs = lookaheadHost * factor;
    delay.setSamples(windowOs);
    for (auto& c : chans)
        c.hold.setSamples(windowOs);
    tapFade = 1.0;
    resetDsp();
    juce::ignoreUnused(channels);
}

void LimiterProcessor::resetDsp()
{
    os.reset();
    delay.reset();
    for (auto& c : chans)
        c.reset();
    tpMeter.reset();
    lmMeter.reset();
    crestPeak = crestMs = 0.0;
    crestCount = 0;
    crestDb = 12.0;
}

void LimiterProcessor::beginBlock(int)
{
    // lookahead changes are integer latency steps, taken between tap crossfades (as the gate and de-esser)
    const int want = juce::jlimit(minLookaheadHost, maxLookaheadHost, static_cast<int>(std::lround(lookaheadRaw.get() * 1e-3 * sr)));
    if (want != lookaheadHost && tapFade >= 1.0)
    {
        oldWindowOs = windowOs;
        lookaheadHost = want;
        windowOs = want * factor;
        delay.setSamples(windowOs);
        for (auto& c : chans)
        {
            c.hold.setSamples(windowOs);
            std::fill(c.avgRing.begin(), c.avgRing.end(), 0.0f);
            c.avgPos = c.avgCount = 0;
            c.avgSum = 0.0;
        }
        tapFade = 0.0;
    }
    if (metersReset.exchange(false))
        lmMeter.reset();
}

float LimiterProcessor::movingAverage(ChannelState& c, float held) noexcept
{
    const int n = windowOs + 1; // the window plus the current sample, as the hold
    if (n <= 1)
        return held;
    const int len = static_cast<int>(c.avgRing.size());
    c.avgSum += held;
    if (c.avgCount < n)
        ++c.avgCount;
    else
    {
        int oldest = c.avgPos - n;
        if (oldest < 0)
            oldest += len;
        c.avgSum -= c.avgRing[static_cast<size_t>(oldest)];
    }
    c.avgRing[static_cast<size_t>(c.avgPos)] = held;
    if (++c.avgPos == len)
        c.avgPos = 0;
    if (c.avgPos == 0) // periodic exact re-sum against drift
    {
        double s = 0.0;
        for (int i = 0; i < c.avgCount; ++i)
        {
            int idx = c.avgPos - 1 - i;
            if (idx < 0)
                idx += len;
            s += c.avgRing[static_cast<size_t>(idx)];
        }
        c.avgSum = s;
    }
    return static_cast<float>(c.avgSum / c.avgCount);
}

void LimiterProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), kChannels);
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), kChannels);
    if (numMain <= 0 || numOut <= 0)
        return;
    const float inStartDb = input->current(), cStartDb = ceiling->current();
    for (auto* s : {input, ceiling, release, link})
        s->skip(n);
    const float inStart = mm::dbToGain(inStartDb), inEnd = mm::dbToGain(input->current());
    const float cStart = mm::dbToGain(cStartDb), cEnd = mm::dbToGain(ceiling->current());
    const float linkAmt = link->current() * 0.01f;
    const auto& chr = kCharacters[juce::jlimit(0, 2, static_cast<int>(std::lround(characterRef.get())))];
    const double scale = static_cast<double>(release->current()) / chr.slowMs; // the labelled release is the slow stage
    // floors: a fast stage under a millisecond is distortion, not release, and at lookahead 0 it would turn the
    // limiter into a clipper whose reconstruction overshoots the ceiling (ADR-0012 amendment)
    const double slowMs = std::max(2.0, chr.slowMs * scale), fastMs = std::max(1.0, chr.fastMs * scale);
    const bool autoOn = autoRelease.on();
    const double slowRel = releaseCoeff(slowMs, osRate), fastRel = releaseCoeff(fastMs, osRate);
    // the slow stage's attack: its own release time (a 10-90 % time: a quarter of it let the slow stage take 16 %
    // of a 5 ms burst's reduction and hold the fast stage's recovery back; ADR-0012 amendment), or by the crest
    // factor when Auto is on (transient material keeps the slow stage out, dense material lets it take over)
    const double crestNorm = juce::jlimit(0.0, 1.0, (crestDb - 6.0) / 9.0);
    const double slowAttackMs = autoOn ? slowMs * (0.05 + 0.95 * crestNorm) : slowMs;
    const double slowAtk = releaseCoeff(slowAttackMs, osRate);
    const int ditherChoice = juce::jlimit(0, 2, static_cast<int>(std::lround(ditherRef.get())));
    for (auto& d : dither)
        d.setBits(ditherChoice == 0 ? 0 : (ditherChoice == 1 ? 16 : 24));

    // input gain, then up
    // a mono output runs the stereo path on a scratch lane: aliasing both pointers to one buffer applied the input
    // gain, the clamp and the dither twice (review finding)
    float* ptrs[kChannels] = {buffer.getWritePointer(0), numOut > 1 ? buffer.getWritePointer(1) : monoLane.data()};
    if (numMain == 1 || numOut == 1)
        std::copy_n(ptrs[0], n, ptrs[1]);
    for (int ch = 0; ch < kChannels; ++ch)
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(n);
            ptrs[ch][i] *= inStart + (inEnd - inStart) * t;
        }
    juce::dsp::AudioBlock<float> block(ptrs, static_cast<size_t>(kChannels), 0, static_cast<size_t>(n));
    auto up = os.up(block);
    float* l = up.getChannelPointer(0);
    float* r = up.getChannelPointer(1);
    const int m = static_cast<int>(up.getNumSamples());
    float maxRed = 0.0f;
    for (int i = 0; i < m; ++i)
    {
        const float t = static_cast<float>(i + 1) / static_cast<float>(m);
        const float c = cStart + (cEnd - cStart) * t;
        const float lvl[kChannels] = {std::abs(l[i]), std::abs(r[i])};
        const float maxLvl = std::max(lvl[0], lvl[1]);
        // crest factor for Auto (50 ms)
        crestPeak = std::max(crestPeak, static_cast<double>(maxLvl));
        crestMs += static_cast<double>(maxLvl) * maxLvl;
        if (++crestCount >= crestWindow)
        {
            const double rms = std::sqrt(crestMs / crestCount);
            crestDb = rms > 1e-9 ? 20.0 * std::log10(crestPeak / rms) : 12.0;
            crestPeak = crestMs = 0.0;
            crestCount = 0;
        }
        float gains[kChannels];
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto& st = chans[static_cast<size_t>(ch)];
            const float det = lvl[ch] + (maxLvl - lvl[ch]) * linkAmt;
            const float required = det > c ? mm::fastGainToDb(det / c) : 0.0f; // reduction needed, dB >= 0
            const float held = st.hold.maxEnvelope(required);                 // the most over the next window
            const float ramp = movingAverage(st, held);                        // reaches `held` as the peak arrives
            // the slow stage follows the ramp with its own attack and release; the fast stage covers the remainder
            // instantly and releases fast; both never let the total under the ramp
            if (ramp > st.slowDb)
                st.slowDb = ramp + (st.slowDb - ramp) * slowAtk;
            else
                st.slowDb = ramp + (st.slowDb - ramp) * slowRel;
            const double rest = std::max(0.0, static_cast<double>(ramp) - st.slowDb);
            st.fastDb = rest > st.fastDb ? rest : rest + (st.fastDb - rest) * fastRel;
            const double total = st.slowDb + st.fastDb;
            maxRed = std::max(maxRed, static_cast<float>(total));
            gains[ch] = total <= 0.0 ? 1.0f : mm::fastDbToGain(-static_cast<float>(total));
        }
        // the audio delayed by the window (two taps while a lookahead change fades)
        delay.writeSignal(l[i], 0);
        delay.writeSignal(r[i], 1);
        float dl = delay.readDelayed(windowOs, 0), dr = delay.readDelayed(windowOs, 1);
        if (tapFade < 1.0)
        {
            dl = static_cast<float>(delay.readDelayed(oldWindowOs, 0) * (1.0 - tapFade) + dl * tapFade);
            dr = static_cast<float>(delay.readDelayed(oldWindowOs, 1) * (1.0 - tapFade) + dr * tapFade);
            tapFade = std::min(1.0, tapFade + tapFadeStep);
        }
        l[i] = dl * gains[0];
        r[i] = dr * gains[1];
    }
    os.down(block);
    // the down-sampler reconstructs a hard-held waveform with up to ~0.4 dB of overshoot on dense material: a
    // sample-peak clamp at the ceiling takes that off (its own ringing is a few hundredths of a dB; ADR-0012 amendment)
    for (int ch = 0; ch < kChannels; ++ch)
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(n);
            const float c = cStart + (cEnd - cStart) * t;
            ptrs[ch][i] = juce::jlimit(-c, c, ptrs[ch][i]);
        }
    // dither last, then the meters read the output
    for (int ch = 0; ch < kChannels; ++ch)
        if (dither[static_cast<size_t>(ch)].isEnabled())
            for (int i = 0; i < n; ++i)
                ptrs[ch][i] = dither[static_cast<size_t>(ch)].process(ptrs[ch][i]);
    const float* outPtrs[kChannels] = {ptrs[0], ptrs[1]};
    const float blockTp = tpMeter.processBlock(outPtrs, n);
    lmMeter.process(outPtrs, n);
    outTp.store(tpMeter.maxDbtp(), std::memory_order_relaxed);
    outLufs.store(lmMeter.integratedLufs(), std::memory_order_relaxed);
    outLra.store(lmMeter.loudnessRangeLu(), std::memory_order_relaxed);
    over.store(blockTp > ceiling->current() + 0.1f, std::memory_order_relaxed);
    reportGainReduction(maxRed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LimiterProcessor(); }
