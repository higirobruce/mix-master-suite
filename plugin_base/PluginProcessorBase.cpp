#include "plugin_base/PluginProcessorBase.h"
#include "core/common/Denormals.h"
#include "plugin_base/StateIO.h"

namespace mm
{
PluginProcessorBase::PluginProcessorBase(const ParamTable& t, const juce::String& id, BusesProperties buses,
                                         PresetManager::FactorySource factory)
    : juce::AudioProcessor(buses), paramTable(t), pluginId(id),
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
        sp.smoother.setTarget(sp.raw->load());
}

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
    prepareDsp(sampleRate, maxBlock, preparedChannels);
    for (auto& sp : smoothers)
    {
        sp.smoother.prepare(sampleRate, sp.rampMs);
        sp.smoother.snap(sp.raw->load());
    }
    dryBuffer.setSize(preparedChannels, maxBlock, false, false, true);
    dryDelay.prepare(juce::jmax(0, maxDspLatencySamples()), preparedChannels);
    dryDelay.setDelay(dspLatencySamples());
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
    setLatencySamples(dspLatencySamples());
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
    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = juce::jmin(getTotalNumOutputChannels(), buffer.getNumChannels(), preparedChannels);
    if (!prepared || numSamples == 0 || numOut == 0)
        return;
    jassert(numSamples <= dryBuffer.getNumSamples());

    // mono -> stereo: copy the mono input to every output channel before processing
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
    for (int ch = numOut; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    updateSmoothers();

    // input metering (pre)
    for (int ch = 0; ch < juce::jmin(numOut, 2); ++ch)
    {
        peakIn[ch].process(buffer.getReadPointer(ch), numSamples);
        rmsIn[ch].process(buffer.getReadPointer(ch), numSamples);
    }
    matchIn.process(buffer.getReadPointer(0), numSamples);

    // dry copy for bypass / delta
    for (int ch = 0; ch < numOut; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    processDsp(buffer, midi);

    // align the dry path with the reported DSP latency
    const int latency = dspLatencySamples();
    if (dryDelay.getDelay() != latency)
        dryDelay.setDelay(latency);
    for (int ch = 0; ch < numOut; ++ch)
    {
        float* d = dryBuffer.getWritePointer(ch);
        for (int n = 0; n < numSamples; ++n)
            d[n] = dryDelay.process(d[n], ch);
    }

    const bool bypassed = hostBypass || bypassRaw->load() >= 0.5f;
    bypassMix.setTarget(bypassed ? 1.0f : 0.0f);
    if (!bypassed)
    {
        matchOut.process(buffer.getReadPointer(0), numSamples);
        const float inLin = matchIn.levelLinear(), outLin = matchOut.levelLinear();
        if (inLin > 1e-6f && outLin > 1e-6f)
            matchGain = juce::jlimit(dbToGain(-24.0f), dbToGain(24.0f), outLin / inLin);
    }

    const bool deltaOn = delta.load();
    auto& trim = smoothers[static_cast<size_t>(smoothedIndexByParam[static_cast<size_t>(trimIndex)])].smoother;
    for (int n = 0; n < numSamples; ++n)
    {
        const float b = bypassMix.next();
        const float trimGain = dbToGain(trim.next());
        for (int ch = 0; ch < numOut; ++ch)
        {
            const float wet = buffer.getSample(ch, n);
            const float dry = dryBuffer.getSample(ch, n);
            float out = deltaOn ? (wet - dry) : (wet * (1.0f - b) + dry * matchGain * b);
            out *= trimGain;
            buffer.setSample(ch, n, out);
        }
    }

    // NaN/Inf guard: never let a non-finite sample reach the host
    bool bad = false;
    for (int ch = 0; ch < numOut && !bad; ++ch)
    {
        const float* p = buffer.getReadPointer(ch);
        for (int n = 0; n < numSamples; ++n)
            if (!std::isfinite(p[n]))
            {
                bad = true;
                break;
            }
    }
    if (bad)
    {
        buffer.clear();
        nanCount.fetch_add(1);
        resetDsp();
    }

    // output metering (post trim) and FIFO publish
    MeterFrame frame;
    for (int ch = 0; ch < juce::jmin(numOut, 2); ++ch)
    {
        peakOut[ch].process(buffer.getReadPointer(ch), numSamples);
        rmsOut[ch].process(buffer.getReadPointer(ch), numSamples);
        frame.peakInDb[ch] = peakIn[ch].levelDb();
        frame.peakOutDb[ch] = peakOut[ch].levelDb();
        frame.rmsInDb[ch] = rmsIn[ch].levelDb();
        frame.rmsOutDb[ch] = rmsOut[ch].levelDb();
    }
    if (numOut == 1)
    {
        frame.peakInDb[1] = frame.peakInDb[0];
        frame.peakOutDb[1] = frame.peakOutDb[0];
        frame.rmsInDb[1] = frame.rmsInDb[0];
        frame.rmsOutDb[1] = frame.rmsOutDb[0];
    }
    frame.latencyMs = static_cast<float>(1000.0 * getLatencySamples() / currentSampleRate);
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
        // missing params -> default first, then apply what is present
        for (const auto& s : paramTable.specs())
            if (auto* p = state.getParameter(s.id))
                p->setValueNotifyingHost(p->getDefaultValue());
        StateIO::applySnapshot(params, state, paramTable, nullptr);
    }
    abcd->fromState(root.getChildWithName("SLOTS"));
    if (auto u = root.getChildWithName("UI"); u.isValid())
        ui.copyPropertiesAndChildrenFrom(u, nullptr);
    if (auto extra = root.getChildWithName("EXTRA"); extra.isValid() && extra.getNumChildren() > 0)
        restoreExtraState(extra.getChild(0));
    if (auto preset = root.getChildWithName("PRESET_NAME"); preset.isValid())
        presetManager->setCurrentName(preset.getProperty("name", "Default").toString());
}
} // namespace mm
