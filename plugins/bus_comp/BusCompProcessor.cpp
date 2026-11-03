#include "plugins/bus_comp/BusCompProcessor.h"
#include "Params.h"
#include "bus_comp_data.h"
#include "core/common/Math.h"
#include "plugins/bus_comp/BusCompEditor.h"
#include <algorithm>
#include <cmath>

namespace bp = mm::plugins::bus_comp;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &bus_comp_data::getNamedResource;
    f.names = bus_comp_data::namedResourceList;
    f.count = bus_comp_data::namedResourceListSize;
    return f;
}
inline int choiceOf(const float v, int count) noexcept { return juce::jlimit(0, count - 1, static_cast<int>(std::lround(v))); }
} // namespace

BusCompProcessor::BusCompProcessor()
    : mm::PluginProcessorBase(bp::table(), bp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                                  .withInput("Sidechain", juce::AudioChannelSet::stereo(), false),
                              factory(), bp::kPluginName)
{
    ratioRef = paramRef(bp::id::ratio);
    attackRef = paramRef(bp::id::attack);
    releaseRef = paramRef(bp::id::release);
    oversampleRef = paramRef(bp::id::oversample);
    scExternal = paramRef(bp::id::sc_external);
    threshold = &smoothed(bp::id::threshold_db);
    makeup = &smoothed(bp::id::makeup_db);
    mix = &smoothed(bp::id::mix_pct);
    scHp = &smoothed(bp::id::sc_hp_hz);
}

juce::AudioProcessorEditor* BusCompProcessor::createEditor() { return new BusCompEditor(*this); }

double BusCompProcessor::ratioValue() const noexcept { return kRatios[choiceOf(ratioRef.get(), 3)]; }
double BusCompProcessor::attackMs() const noexcept { return kAttacksMs[choiceOf(attackRef.get(), 6)]; }
double BusCompProcessor::releaseMs() const noexcept
{
    const int i = choiceOf(releaseRef.get(), 5);
    return i == 4 ? -1.0 : kReleasesMs[i];
}

float BusCompProcessor::staticGainDb(float levelDb) noexcept
{
    mm::GainComputerParams gp;
    gp.thresholdDb = apvts().getRawParameterValue(bp::id::threshold_db)->load();
    gp.ratio = ratioValue();
    gp.kneeDb = kKneeDb;
    mm::GainComputer<float> g;
    g.set(gp);
    return g.gainDb(levelDb);
}

// ---- chain

void BusCompProcessor::Chain::prepare(double rate)
{
    for (auto& f : sc)
        f.prepare(rate);
    detector.prepare(rate);
    detector.setMode(mm::DetectorMode::Peak);
    env.prepare(rate);
    reset();
}

void BusCompProcessor::Chain::reset()
{
    for (auto& f : sc)
        f.reset();
    detector.reset();
    env.reset();
    env.snapTo(0.0f);
}

float BusCompProcessor::Chain::gain(float keyL, float keyR, const mm::GainComputer<float>& g, float& red) noexcept
{
    const float k = 0.5f * (std::abs(sc[0].process(keyL)) + std::abs(sc[1].process(keyR))); // linked: the summed rectified key
    const float lvlDb = mm::fastGainToDb(detector.process(k));
    red = env.process(-g.gainDb(lvlDb));
    return red <= 0.0f ? 1.0f : mm::fastDbToGain(-red);
}

void BusCompProcessor::configure(Chain& c) noexcept
{
    for (auto& f : c.sc)
        f.setHighPass(scHp->current() >= 20.0f ? static_cast<double>(scHp->current()) : 0.0);
    c.env.setAttackMs(attackMs());
    const double rel = releaseMs();
    c.env.setAutoRelease(rel < 0.0);
    c.env.setReleaseMs(rel < 0.0 ? 100.0 : rel);
}

// ---- lifecycle

void BusCompProcessor::prepareDsp(double sampleRate, int maxBlock, int channels)
{
    sr = sampleRate;
    maxBlockSamples = maxBlock;
    os.prepare(sr, maxBlock, kChannels, 2);
    keyOs.prepare(sr, maxBlock, kChannels, 2);
    osLatency = os.latencySamples();
    plain.prepare(sr);
    over.prepare(2.0 * sr);
    mixDelay.prepare(osLatency, kChannels, maxBlock);
    for (auto* v : {&wetA, &wetB, &dry})
        for (auto& b : *v)
            b.assign(static_cast<size_t>(maxBlock), 0.0f);
    for (auto& b : keyUp)
        b.assign(static_cast<size_t>(2 * maxBlock), 0.0f);
    snapSmoothersToTargets();
    activePath = oversampleRef.on() ? 1 : 0;
    pendingPath = -1;
    warmupRemaining = fadeRemaining = 0;
    reportedLatency = activePath == 1 ? osLatency : 0;
    mixDelay.setDelay(reportedLatency);
    rampsValid = false;
    resetDsp();
    juce::ignoreUnused(channels);
}

void BusCompProcessor::resetDsp()
{
    plain.reset();
    over.reset();
    os.reset();
    keyOs.reset();
    mixDelay.reset();
}

void BusCompProcessor::beginBlock(int)
{
    // an oversampling switch: the incoming path starts now; the oversampled one warms up for its latency, then a
    // 10 ms crossfade hands over and the latency is re-reported at its end
    const int wanted = oversampleRef.on() ? 1 : 0;
    if (wanted != activePath && pendingPath < 0)
    {
        pendingPath = wanted;
        warmupRemaining = wanted == 1 ? osLatency : 0;
        fadeTotal = std::max(1, static_cast<int>(std::lround(kFadeMs * 1e-3 * sr)));
        fadeRemaining = fadeTotal;
        if (wanted == 1)
        {
            over.reset();
            os.reset();
            keyOs.reset();
        }
        else
            plain.reset();
    }
}

void BusCompProcessor::runPlain(const float* const* in, const float* const* key, int keyChannels, float* const* out, int n, int numMain, float& maxRed) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const float l = in[0][i], r = in[numMain > 1 ? 1 : 0][i];
        const float kl = keyChannels > 0 ? key[0][i] : l, kr = keyChannels > 0 ? key[keyChannels > 1 ? 1 : 0][i] : r;
        float red = 0.0f;
        const float g = plain.gain(kl, kr, gc, red);
        maxRed = std::max(maxRed, red);
        out[0][i] = l * g;
        out[1][i] = r * g;
    }
}

void BusCompProcessor::runOversampled(const float* const* in, const float* const* key, int keyChannels, float* const* out, int n, int numMain, float& maxRed) noexcept
{
    // copy the input into the output lanes, up-sample in place, apply the gain at 2x, down-sample
    for (int ch = 0; ch < kChannels; ++ch)
        std::copy_n(in[numMain > 1 ? ch : 0], n, out[ch]);
    juce::dsp::AudioBlock<float> block(const_cast<float* const*>(out), static_cast<size_t>(kChannels), 0, static_cast<size_t>(n));
    auto up = os.up(block);
    const float* keyPtr[2] = {up.getChannelPointer(0), up.getChannelPointer(1)};
    if (keyChannels > 0)
    {
        for (int ch = 0; ch < kChannels; ++ch)
            std::copy_n(key[keyChannels > 1 ? ch : 0], n, keyUp[static_cast<size_t>(ch)].data());
        float* kp[2] = {keyUp[0].data(), keyUp[1].data()};
        juce::dsp::AudioBlock<float> kblock(kp, static_cast<size_t>(kChannels), 0, static_cast<size_t>(n));
        auto kup = keyOs.up(kblock);
        keyPtr[0] = kup.getChannelPointer(0);
        keyPtr[1] = kup.getChannelPointer(1);
    }
    float* l = up.getChannelPointer(0);
    float* r = up.getChannelPointer(1);
    const int m = static_cast<int>(up.getNumSamples());
    for (int i = 0; i < m; ++i)
    {
        float red = 0.0f;
        const float g = over.gain(keyPtr[0][i], keyPtr[1][i], gc, red);
        maxRed = std::max(maxRed, red);
        l[i] *= g;
        r[i] *= g;
    }
    os.down(block);
}

void BusCompProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), kChannels);
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), kChannels);
    if (numMain <= 0 || numOut <= 0)
        return;
    for (auto* s : {threshold, makeup, mix, scHp})
        s->skip(n);
    mm::GainComputerParams gp;
    gp.thresholdDb = threshold->current();
    gp.ratio = ratioValue();
    gp.kneeDb = kKneeDb;
    gc.set(gp);
    configure(plain);
    configure(over);
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
    // dry copies (the mix side, delayed to the reported latency) and the input lanes
    const float* in[kChannels] = {buffer.getReadPointer(0), buffer.getReadPointer(numMain > 1 ? 1 : 0)};
    for (int ch = 0; ch < kChannels; ++ch)
    {
        std::copy_n(in[ch], n, dry[static_cast<size_t>(ch)].data());
        mixDelay.writeBlock(in[ch], n, ch);
        mixDelay.readBlock(dry[static_cast<size_t>(ch)].data(), n, mixDelay.getDelay(), ch);
    }
    float* a[kChannels] = {wetA[0].data(), wetA[1].data()};
    float* b[kChannels] = {wetB[0].data(), wetB[1].data()};
    float maxRed = 0.0f;
    // the active path into A; a pending path into B
    if (activePath == 0)
        runPlain(in, key, keyChannels, a, n, numMain, maxRed);
    else
        runOversampled(in, key, keyChannels, a, n, numMain, maxRed);
    const float* wet[kChannels] = {a[0], a[1]};
    if (pendingPath >= 0)
    {
        float redB = 0.0f;
        if (pendingPath == 0)
            runPlain(in, key, keyChannels, b, n, numMain, redB);
        else
            runOversampled(in, key, keyChannels, b, n, numMain, redB);
        // warm-up, then the crossfade from A to B written into A
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
            // hand over: B becomes the active path (its chain state continues), and the latency follows
            activePath = pendingPath;
            pendingPath = -1;
            reportedLatency = activePath == 1 ? osLatency : 0;
            mixDelay.setDelay(reportedLatency);
            maxRed = std::max(maxRed, redB);
            // the chain objects are per path, so nothing to swap: the next block runs the new active path into A
        }
    }
    // makeup and mix ramp per sample from last block's values
    const float mkEnd = mm::dbToGain(makeup->current());
    const float mixEnd = mix->current() * 0.01f;
    if (!rampsValid)
    {
        lastMakeupGain = mkEnd;
        lastMix = mixEnd;
        rampsValid = true;
    }
    const float mkStart = lastMakeupGain, mixStart = lastMix;
    lastMakeupGain = mkEnd;
    lastMix = mixEnd;
    for (int ch = 0; ch < numOut; ++ch)
    {
        float* outPtr = buffer.getWritePointer(ch);
        const float* w = wet[juce::jmin(ch, kChannels - 1)];
        const float* d = dry[static_cast<size_t>(juce::jmin(ch, kChannels - 1))].data();
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(n);
            const float mk = mkStart + (mkEnd - mkStart) * t;
            const float mx = mixStart + (mixEnd - mixStart) * t;
            outPtr[i] = d[i] + (w[i] * mk - d[i]) * mx;
        }
    }
    reportGainReduction(maxRed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BusCompProcessor(); }
