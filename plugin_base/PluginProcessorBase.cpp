#include "plugin_base/PluginProcessorBase.h"
#include "core/common/Denormals.h"
#include "plugin_base/StateIO.h"

namespace mm
{
PluginProcessorBase::PluginProcessorBase(const ParamTable& t, const juce::String& id, BusesProperties buses,
                                         PresetManager::FactorySource factory, const juce::String& displayName)
    : juce::AudioProcessor(buses), paramTable(t), pluginId(id), pluginName(displayName.isEmpty() ? id : displayName),
      state(*this, &undoManager, juce::Identifier(id), t.makeLayout())
{
    abcd = std::make_unique<AbcdSlots>(state, paramTable, &undoManager);
    presetManager = std::make_unique<PresetManager>(state, paramTable, pluginId, &undoManager, factory);

    const auto specs = paramTable.specs();
    smoothers.reserve(specs.size());
    smoothedIndexByParam.assign(specs.size(), -1);
    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& s = specs[i];
        if (s.type == ParamType::Float && s.smoothingMs > 0.0f)
        {
            SmoothedParam sp;
            sp.raw = state.getRawParameterValue(s.id);
            sp.rampMs = s.smoothingMs;
            sp.smoother.prepare(48000.0, s.smoothingMs);
            sp.smoother.snap(sp.raw != nullptr ? sp.raw->load() : s.def);
            smoothers.push_back(std::move(sp));
            smoothedIndexByParam[i] = static_cast<int>(smoothers.size() - 1);
        }
        if (juce::String(s.id) == "bypass")
            bypassRaw = state.getRawParameterValue(s.id);
        if (juce::String(s.id) == "trim_db")
            trimIndex = static_cast<int>(i);
    }
    jassert(bypassRaw != nullptr && trimIndex >= 0 && "every plugin has the reserved bypass/trim_db params");
    trimSmoother = &smoothers[static_cast<size_t>(smoothedIndexByParam[static_cast<size_t>(trimIndex)])].smoother;
}

PluginProcessorBase::ParamRef PluginProcessorBase::paramRef(ParamId id) const noexcept
{
    return ParamRef{state.getRawParameterValue(id)};
}

PluginProcessorBase::~PluginProcessorBase() = default;

float PluginProcessorBase::param(ParamId id) const noexcept
{
    if (auto* raw = state.getRawParameterValue(id))
        return raw->load();
    return 0.0f;
}

Smoother<float>& PluginProcessorBase::smoothed(ParamId id) noexcept
{
    const int idx = paramTable.index(id);
    jassert(idx >= 0 && smoothedIndexByParam[static_cast<size_t>(idx)] >= 0 && "parameter is not a smoothed float");
    return smoothers[static_cast<size_t>(smoothedIndexByParam[static_cast<size_t>(idx)])].smoother;
}

void PluginProcessorBase::updateSmoothers() noexcept
{
    for (auto& sp : smoothers)
        sp.smoother.setTarget(sp.raw->load(std::memory_order_relaxed));
}

void PluginProcessorBase::snapSmoothersToTargets() noexcept
{
    for (auto& sp : smoothers)
        sp.smoother.snap(sp.raw->load(std::memory_order_relaxed));
}

namespace
{
// Sum of squares with four accumulators (vectorises); returns a float so a NaN/Inf poisons the result.
inline float sumSquares(const float* p, int n) noexcept
{
    float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    int i = 0;
    for (; i + 4 <= n; i += 4)
    {
        a0 += p[i] * p[i];
        a1 += p[i + 1] * p[i + 1];
        a2 += p[i + 2] * p[i + 2];
        a3 += p[i + 3] * p[i + 3];
    }
    for (; i < n; ++i)
        a0 += p[i] * p[i];
    return (a0 + a1) + (a2 + a3);
}
} // namespace

bool PluginProcessorBase::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    if (!inOk || !outOk || in.size() > out.size())
        return false;
    for (int b = 1; b < layouts.inputBuses.size(); ++b) // sidechain buses: disabled, mono or stereo
    {
        const auto sc = layouts.inputBuses[b];
        if (!(sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo()))
            return false;
    }
    return true;
}

void PluginProcessorBase::prepareToPlay(double sampleRate, int maxBlock)
{
    currentSampleRate = sampleRate;
    preparedChannels = juce::jmax(1, getTotalNumOutputChannels());
    // smoothers first, so prepareDsp() designs from the current parameter values (no ramp in the first block)
    for (auto& sp : smoothers)
    {
        sp.smoother.prepare(sampleRate, sp.rampMs);
        sp.smoother.snap(sp.raw->load());
    }
    prepareDsp(sampleRate, maxBlock, preparedChannels);
    dryBuffer.setSize(preparedChannels, maxBlock, false, false, true);
    dryDelay.prepare(juce::jmax(0, maxDspLatencySamples()), preparedChannels);
    reportedLatency = dspLatencySamples();
    dryDelay.setDelay(reportedLatency);
    dryDelay.reset();
    bypassMix.prepare(sampleRate, 20.0);
    bypassMix.snap(bypassRaw->load() >= 0.5f ? 1.0f : 0.0f);
    matchGain = 1.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        peakIn[ch].prepare(sampleRate);
        peakOut[ch].prepare(sampleRate);
        rmsIn[ch].prepare(sampleRate);
        rmsOut[ch].prepare(sampleRate);
    }
    matchIn.prepare(sampleRate, 400.0);
    matchOut.prepare(sampleRate, 400.0);
    setLatencySamples(reportedLatency);
    prepared = true;
}

void PluginProcessorBase::releaseResources()
{
    prepared = false;
    resetDsp();
}

void PluginProcessorBase::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    processInternal(buffer, midi, false);
}

void PluginProcessorBase::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    processInternal(buffer, midi, true);
}

void PluginProcessorBase::processInternal(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hostBypass)
{
    ScopedNoDenormals noDenormals;
    if (!prepared || buffer.getNumSamples() == 0)
        return;
    // Hosts may deliver more samples than prepareToPlay announced: process in sub-blocks that fit our buffers.
    const int maxBlock = dryBuffer.getNumSamples();
    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin(maxBlock, total - start);
        juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, n);
        processSubBlock(view, midi, hostBypass);
    }
}

void PluginProcessorBase::resetMeters() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        peakIn[ch].reset();
        peakOut[ch].reset();
        rmsIn[ch].reset();
        rmsOut[ch].reset();
    }
    matchIn.reset();
    matchOut.reset();
    matchGain = 1.0f;
}

void PluginProcessorBase::processSubBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hostBypass)
{
    const int numSamples = buffer.getNumSamples();
    const int numIn = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels());   // main bus only
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), preparedChannels);
    if (numOut == 0 || numIn == 0)
        return;
    jassert(numSamples <= dryBuffer.getNumSamples());

    updateSmoothers();
    beginBlock(numSamples);

    // input metering (pre) on the main input channels; a non-finite host block is silenced before it reaches the DSP
    const int meterIn = juce::jmin(numIn, 2);
    bool badInput = false;
    float inSq[2] = {0.0f, 0.0f};
    for (int ch = 0; ch < meterIn; ++ch)
    {
        inSq[ch] = sumSquares(buffer.getReadPointer(ch), numSamples);
        if (!std::isfinite(inSq[ch]))
            badInput = true;
    }
    if (badInput)
    {
        for (int ch = 0; ch < numIn; ++ch)
            buffer.clear(ch, 0, numSamples);
        nanCount.fetch_add(1);
        resetMeters();
        inSq[0] = inSq[1] = 0.0f;
    }
    for (int ch = 0; ch < meterIn; ++ch)
    {
        peakIn[ch].processPeak(buffer.getMagnitude(ch, 0, numSamples), numSamples);
        rmsIn[ch].processSumSquares(inSq[ch], numSamples);
    }
    matchIn.processSumSquares(inSq[0], numSamples);

    // bypass state first, then the dry copy (always: it is the bypass/delta source and must be current)
    const bool bypassed = hostBypass || bypassRaw->load() >= 0.5f;
    const bool deltaOn = delta.load();
    bypassMix.setTarget(bypassed ? 1.0f : 0.0f);
    for (int ch = 0; ch < numOut; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, juce::jmin(ch, numIn - 1), 0, numSamples);

    // latency may change at runtime (oversampling / lookahead): report it and keep the dry path aligned
    const int latency = dspLatencySamples();
    jassert(latency <= maxDspLatencySamples() && "maxDspLatencySamples() must bound every runtime latency");
    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        dryDelay.setDelay(latency);
        setLatencySamples(latency);
    }

    processDsp(buffer, midi);

    // mono -> stereo: the processed mono channel feeds every main output (unless the plugin made its own stereo)
    if (!producesStereoFromMono())
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);

    if (latency > 0)
        for (int ch = 0; ch < numOut; ++ch)
        {
            float* d = dryBuffer.getWritePointer(ch);
            for (int n = 0; n < numSamples; ++n)
                d[n] = dryDelay.process(d[n], ch);
        }

    // wet level (pre-trim, channel 0) for gain matching; applied only if the block turns out finite
    const float wetSq = sumSquares(buffer.getReadPointer(0), numSamples);

    // output stage: bypass crossfade / delta, then trim (block-rate ramps of the dB smoothers)
    auto& trim = *trimSmoother;
    const float trimStart = trimGain.get(trim.current());
    trim.skip(numSamples);
    const float trimEnd = trimGain.get(trim.current());
    const bool mixing = deltaOn || bypassMix.isSmoothing() || bypassMix.current() > 0.0f;
    if (mixing)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float b = bypassMix.next();
            for (int ch = 0; ch < numOut; ++ch)
            {
                const float wet = buffer.getSample(ch, n);
                const float dry = dryBuffer.getSample(ch, n);
                buffer.setSample(ch, n, deltaOn ? (wet - dry) : (wet * (1.0f - b) + dry * matchGain * b));
            }
        }
    }
    if (trimStart != 1.0f || trimEnd != 1.0f)
        for (int ch = 0; ch < numOut; ++ch)
            buffer.applyGainRamp(ch, 0, numSamples, trimStart, trimEnd);

    // NaN/Inf guard (a non-finite sample poisons the sum of squares) and output metering in one pass per channel
    float outSq[2] = {0.0f, 0.0f};
    bool bad = !std::isfinite(wetSq);
    for (int ch = 0; ch < numOut; ++ch)
    {
        const float sq = sumSquares(buffer.getReadPointer(ch), numSamples);
        if (!std::isfinite(sq))
            bad = true;
        if (ch < 2)
            outSq[ch] = sq;
    }
    if (bad)
    {
        for (int ch = 0; ch < numOut; ++ch)
            buffer.clear(ch, 0, numSamples);
        nanCount.fetch_add(1);
        resetDsp();
        resetMeters();
        outSq[0] = outSq[1] = 0.0f;
    }
    else // wet is computed even while bypassed, so the match ratio stays current for the next un-bypass
    {
        matchOut.processSumSquares(wetSq, numSamples);
        const float inLin = matchIn.levelLinear(), outLin = matchOut.levelLinear();
        if (inLin > 1e-6f && outLin > 1e-6f)
            matchGain = juce::jlimit(0.0630957f /* -24 dB */, 15.8489f /* +24 dB */, outLin / inLin);
    }

    MeterFrame frame;
    const int meterOut = juce::jmin(numOut, 2);
    for (int ch = 0; ch < meterOut; ++ch)
    {
        peakOut[ch].processPeak(bad ? 0.0f : buffer.getMagnitude(ch, 0, numSamples), numSamples);
        rmsOut[ch].processSumSquares(outSq[ch], numSamples);
        frame.peakInDb[ch] = peakIn[juce::jmin(ch, meterIn - 1)].levelDb();
        frame.peakOutDb[ch] = peakOut[ch].levelDb();
        frame.rmsInDb[ch] = rmsIn[juce::jmin(ch, meterIn - 1)].levelDb();
        frame.rmsOutDb[ch] = rmsOut[ch].levelDb();
    }
    if (meterOut == 1)
    {
        frame.peakInDb[1] = frame.peakInDb[0];
        frame.peakOutDb[1] = frame.peakOutDb[0];
        frame.rmsInDb[1] = frame.rmsInDb[0];
        frame.rmsOutDb[1] = frame.rmsOutDb[0];
    }
    frame.latencyMs = static_cast<float>(1000.0 * latency / currentSampleRate);
    frame.gainReductionDb = gainReduction.load(std::memory_order_relaxed);
    meters.push(frame);
}

void PluginProcessorBase::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree root("MM_STATE");
    root.setProperty("version", 1, nullptr);
    root.setProperty("pluginId", pluginId, nullptr);
    root.addChild(StateIO::snapshot(state, paramTable), -1, nullptr);
    root.addChild(abcd->toState(), -1, nullptr);
    root.addChild(ui.createCopy(), -1, nullptr);
    juce::ValueTree extra("EXTRA");
    if (auto e = extraState(); e.isValid())
        extra.addChild(e, -1, nullptr);
    root.addChild(extra, -1, nullptr);
    juce::ValueTree preset("PRESET_NAME");
    preset.setProperty("name", presetManager->currentName(), nullptr);
    root.addChild(preset, -1, nullptr);
    juce::MemoryOutputStream os(destData, false);
    root.writeToStream(os);
}

void PluginProcessorBase::setStateInformation(const void* data, int sizeInBytes)
{
    auto root = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!root.isValid() || !root.hasType("MM_STATE"))
        return;
    if (root.getProperty("pluginId").toString() != pluginId)
        return;
    if (auto params = root.getChildWithName("PARAMETERS"); params.isValid())
    {
        // one set per parameter: the stored value when present, the default when missing (unknown ids ignored)
        for (const auto& spec : paramTable.specs())
        {
            auto* p = state.getParameter(spec.id);
            if (p == nullptr)
                continue;
            float norm = p->getDefaultValue();
            for (const auto& stored : params)
                if (stored.hasType("PARAM") && stored["id"].toString() == spec.id)
                {
                    const float real = juce::jlimit(spec.min, spec.max, static_cast<float>(static_cast<double>(stored["value"])));
                    norm = p->convertTo0to1(real);
                    break;
                }
            p->setValueNotifyingHost(norm);
        }
    }
    abcd->fromState(root.getChildWithName("SLOTS"));
    if (auto u = root.getChildWithName("UI"); u.isValid())
        ui.copyPropertiesAndChildrenFrom(u, nullptr);
    if (auto extra = root.getChildWithName("EXTRA"); extra.isValid() && extra.getNumChildren() > 0)
        restoreExtraState(extra.getChild(0));
    if (auto preset = root.getChildWithName("PRESET_NAME"); preset.isValid())
        presetManager->setCurrentName(preset.getProperty("name", "Default").toString());
    // Tell the host every parameter may have changed (CLAP hosts require a rescan request after a state load).
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true).withNonParameterStateChanged(true));
}
} // namespace mm
