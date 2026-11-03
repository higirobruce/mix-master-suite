#include "plugins/gate/GateProcessor.h"
#include "Params.h"
#include "gate_data.h"
#include "plugins/gate/GateEditor.h"
#include <cmath>

namespace gp = mm::plugins::gate;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &gate_data::getNamedResource;
    f.names = gate_data::namedResourceList;
    f.count = gate_data::namedResourceListSize;
    return f;
}
} // namespace

GateProcessor::GateProcessor()
    : mm::PluginProcessorBase(gp::table(), gp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withInput("Sidechain", juce::AudioChannelSet::mono(), false)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), gp::kPluginName)
{
    mode = paramRef(gp::id::mode);
    scHpEnabled = paramRef(gp::id::sc_hp_enabled);
    scLpEnabled = paramRef(gp::id::sc_lp_enabled);
    extKey = paramRef(gp::id::ext_key);
    listenSc = paramRef(gp::id::listen_sc);
    threshold = &smoothed(gp::id::threshold_db);
    range = &smoothed(gp::id::range_db);
    attack = &smoothed(gp::id::attack_ms);
    hold = &smoothed(gp::id::hold_ms);
    release = &smoothed(gp::id::release_ms);
    ratio = &smoothed(gp::id::ratio);
    hysteresis = &smoothed(gp::id::hysteresis_db);
    lookaheadRaw = paramRef(gp::id::lookahead_ms); // unsmoothed: an integer latency, applied per block (ADR-0005)
    scHp = &smoothed(gp::id::sc_hp_hz);
    scLp = &smoothed(gp::id::sc_lp_hz);
}

bool GateProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!mm::PluginProcessorBase::isBusesLayoutSupported(layouts))
        return false;
    return layouts.inputBuses.size() == 2; // main + sidechain (disabled, mono or stereo)
}

void GateProcessor::prepareDsp(double sampleRate, int, int)
{
    sr = sampleRate;
    maxLookaheadSamples = static_cast<int>(std::lround(kMaxLookaheadMs * 1e-3 * sr));
    lookahead.prepare(sr, kMaxLookaheadMs, kMaxChannels);
    lookahead.setMs(lookaheadRaw.get());
    oldWindow = lookahead.latencySamples();
    tapFade = 1.0;
    tapFadeStep = 1.0 / juce::jmax(1.0, 0.005 * sr);
    peakHold.prepare(sr, kPeakHoldMs, 1);
    peakHold.setMs(kPeakHoldMs);
    for (auto& f : scFilter)
        f.prepare(sr);
    env.prepare(sr);
    resetDsp();
}

void GateProcessor::resetDsp()
{
    for (auto& f : scFilter)
        f.reset();
    lookahead.reset();
    peakHold.reset();
    env.reset();
    env.snapTo(range->current()); // start closed
    state = State::Closed;
    holdLeft = 0;
    openFlag.store(false, std::memory_order_relaxed);
    reportGainReduction(-range->current());
}

void GateProcessor::beginBlock(int)
{
    const int want = juce::jlimit(0, lookahead.maxWindowSamples(), static_cast<int>(std::lround(lookaheadRaw.get() * 1e-3 * sr)));
    // a change that lands inside the 5 ms tap crossfade waits for it (M4 review: restarting the fade dropped the
    // half-faded tap); the reported latency stays on the tap being heard until the next change is taken
    if (want != lookahead.latencySamples() && tapFade >= 1.0)
    {
        oldWindow = lookahead.latencySamples();
        lookahead.setSamples(want);
        tapFade = 0.0;
    }
}

void GateProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), kMaxChannels, buffer.getNumChannels());
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), kMaxChannels, buffer.getNumChannels());
    if (numMain <= 0 || numOut <= 0)
        return;

    for (auto* s : {threshold, range, attack, hold, release, ratio, hysteresis, scHp, scLp})
        s->skip(n);
    const float T = threshold->current();
    const float rangeDb = range->current();
    const float hyst = hysteresis->current();
    const bool expander = mode.get() >= 0.5f;
    const bool listen = listenSc.on();
    const int holdSamples = static_cast<int>(std::lround(hold->current() * 1e-3 * sr));
    env.setAttackMs(attack->current());
    env.setReleaseMs(release->current());
    const int window = lookahead.latencySamples();
    const bool passThrough = window == 0 && tapFade >= 1.0; // no delay to read: the output sample is the input sample
    const bool scActive = scHpEnabled.on() || scLpEnabled.on();
    const float rangeGain = mm::fastDbToGain(rangeDb);
    for (auto& f : scFilter)
    {
        f.setHighPass(scHpEnabled.on() ? scHp->current() : 0.0);
        f.setLowPass(scLpEnabled.on() ? scLp->current() : 0.0);
    }
    mm::GainComputerParams gcp;
    gcp.expander = true;
    gcp.thresholdDb = T;
    gcp.ratio = ratio->current();
    gcp.rangeDb = rangeDb;
    gc.set(gcp);

    // external key: the sidechain bus when the host enabled it; otherwise the internal key
    const float* key[kMaxChannels] = {nullptr, nullptr};
    int keyChannels = 0;
    const auto* scBus = getBus(true, 1);
    if (extKey.on() && scBus != nullptr && scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
    {
        auto scBuffer = getBusBuffer(buffer, true, 1);
        keyChannels = juce::jmin(scBuffer.getNumChannels(), kMaxChannels);
        for (int ch = 0; ch < keyChannels; ++ch)
            key[ch] = scBuffer.getReadPointer(ch);
    }

    float maxReduction = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float keySample[kMaxChannels] = {0.0f, 0.0f};
        float keyPeak = 0.0f;
        for (int ch = 0; ch < numMain; ++ch)
        {
            float k = keyChannels > 0 ? key[juce::jmin(ch, keyChannels - 1)][i] : buffer.getSample(ch, i);
            if (scActive)
                k = scFilter[static_cast<size_t>(ch)].process(k);
            keySample[ch] = k;
            keyPeak = juce::jmax(keyPeak, std::abs(k));
        }
        const float levelDb = mm::fastGainToDb(peakHold.maxEnvelope(lookahead.maxEnvelope(keyPeak)));

        // state machine (ADR-0005): open at T, leave Open below T - hysteresis, hold, then close
        if (levelDb >= T)
        {
            state = State::Open;
            holdLeft = holdSamples;
        }
        else
        {
            if (state == State::Open && levelDb < T - hyst)
                state = State::Hold;
            if (state == State::Hold)
            {
                if (holdLeft > 0)
                    --holdLeft;
                else
                    state = State::Closed;
            }
        }
        const float targetDb = state != State::Closed ? 0.0f : (expander ? gc.gainDb(levelDb) : rangeDb);
        const float gainDb = juce::jlimit(rangeDb, 0.0f, env.process(targetDb));
        maxReduction = juce::jmax(maxReduction, -gainDb);
        const float g = gainDb >= 0.0f ? 1.0f : (gainDb <= rangeDb ? rangeGain : mm::fastDbToGain(gainDb)); // settled: exact
        if (tapFade < 1.0)
            tapFade = juce::jmin(1.0, tapFade + tapFadeStep);
        for (int ch = 0; ch < numOut; ++ch)
        {
            const float x = buffer.getSample(ch, i);
            lookahead.writeSignal(x, ch); // history for a later lookahead increase
            float delayed = passThrough ? x : lookahead.readDelayed(window, ch);
            if (tapFade < 1.0)
                delayed = static_cast<float>(lookahead.readDelayed(oldWindow, ch) * (1.0 - tapFade) + delayed * tapFade);
            buffer.setSample(ch, i, listen ? keySample[juce::jmin(ch, numMain - 1)] : delayed * g);
        }
    }
    openFlag.store(state != State::Closed, std::memory_order_relaxed);
    reportGainReduction(maxReduction);
}

float GateProcessor::staticGainDb(float levelDb) noexcept
{
    const float T = apvts().getRawParameterValue(gp::id::threshold_db)->load();
    const float rangeDb = apvts().getRawParameterValue(gp::id::range_db)->load();
    if (apvts().getRawParameterValue(gp::id::mode)->load() < 0.5f)
        return levelDb >= T ? 0.0f : rangeDb;
    mm::GainComputerParams gcp;
    gcp.expander = true;
    gcp.thresholdDb = T;
    gcp.ratio = apvts().getRawParameterValue(gp::id::ratio)->load();
    gcp.rangeDb = rangeDb;
    mm::GainComputer<float> staticGc;
    staticGc.set(gcp);
    return staticGc.gainDb(levelDb);
}

juce::AudioProcessorEditor* GateProcessor::createEditor() { return new GateEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GateProcessor(); }
