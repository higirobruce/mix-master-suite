#include "plugins/channel_comp/ChannelCompProcessor.h"
#include "Params.h"
#include "channel_comp_data.h"
#include "plugins/channel_comp/ChannelCompEditor.h"
#include <cmath>

namespace cp = mm::plugins::channel_comp;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &channel_comp_data::getNamedResource;
    f.names = channel_comp_data::namedResourceList;
    f.count = channel_comp_data::namedResourceListSize;
    return f;
}
} // namespace

ChannelCompProcessor::ChannelCompProcessor()
    : mm::PluginProcessorBase(cp::table(), cp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withInput("Sidechain", juce::AudioChannelSet::mono(), false)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), cp::kPluginName)
{
    ratioInf = paramRef(cp::id::ratio_inf);
    autoRelease = paramRef(cp::id::auto_release);
    detectorMode = paramRef(cp::id::detector);
    topology = paramRef(cp::id::topology);
    scHpEnabled = paramRef(cp::id::sc_hp_enabled);
    extKey = paramRef(cp::id::ext_key);
    autoMakeup = paramRef(cp::id::auto_makeup);
    listenSc = paramRef(cp::id::listen_sc);
    threshold = &smoothed(cp::id::threshold_db);
    ratio = &smoothed(cp::id::ratio);
    attack = &smoothed(cp::id::attack_ms);
    release = &smoothed(cp::id::release_ms);
    knee = &smoothed(cp::id::knee_db);
    mix = &smoothed(cp::id::mix_pct);
    scHp = &smoothed(cp::id::sc_hp_hz);
    makeup = &smoothed(cp::id::makeup_db);
    link = &smoothed(cp::id::stereo_link_pct);
}

bool ChannelCompProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!mm::PluginProcessorBase::isBusesLayoutSupported(layouts))
        return false;
    return layouts.inputBuses.size() == 2; // main + sidechain (disabled, mono or stereo)
}

void ChannelCompProcessor::prepareDsp(double sampleRate, int, int)
{
    sr = sampleRate;
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        scFilter[static_cast<size_t>(ch)].prepare(sr);
        detector[static_cast<size_t>(ch)].prepare(sr);
        detector[static_cast<size_t>(ch)].setRmsWindowMs(50.0);
        env[static_cast<size_t>(ch)].prepare(sr);
        env[static_cast<size_t>(ch)].snapTo(0.0f);
    }
    resetDsp();
}

void ChannelCompProcessor::resetDsp()
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        scFilter[static_cast<size_t>(ch)].reset();
        detector[static_cast<size_t>(ch)].reset();
        env[static_cast<size_t>(ch)].reset();
        env[static_cast<size_t>(ch)].snapTo(0.0f);
        lastOut[static_cast<size_t>(ch)] = 0.0f;
    }
    reportGainReduction(0.0f);
}

void ChannelCompProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), kMaxChannels, buffer.getNumChannels());
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), kMaxChannels, buffer.getNumChannels());
    if (numMain <= 0 || numOut <= 0)
        return;

    // block-rate parameter reads (smoothers advanced to the block end)
    threshold->skip(n);
    ratio->skip(n);
    attack->skip(n);
    release->skip(n);
    knee->skip(n);
    mix->skip(n);
    scHp->skip(n);
    link->skip(n);
    const float makeupStartDb = makeup->current();
    makeup->skip(n);
    const bool inf = ratioInf.on();
    const double r = inf ? 1000.0 : static_cast<double>(ratio->current());
    mm::GainComputerParams gp;
    gp.thresholdDb = threshold->current();
    gp.ratio = r;
    gp.kneeDb = knee->current();
    gc.set(gp);
    const bool rms = detectorMode.get() >= 0.5f;
    const bool feedback = topology.on();
    const bool scOn = scHpEnabled.on();
    const float linkAmt = link->current() * 0.01f;
    const float mixAmt = mix->current() * 0.01f;
    const bool listen = listenSc.on();
    const bool autoMk = autoMakeup.on();
    const float autoMakeupDb = autoMk ? static_cast<float>(std::abs(gp.thresholdDb) * (1.0 - 1.0 / r) * 0.5) : 0.0f;
    const float mkStart = mm::dbToGain((autoMk ? 0.0f : makeupStartDb) + autoMakeupDb);
    const float mkEnd = mm::dbToGain((autoMk ? 0.0f : makeup->current()) + autoMakeupDb);
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        auto& d = detector[static_cast<size_t>(ch)];
        d.setMode(rms ? mm::DetectorMode::Rms : mm::DetectorMode::Peak);
        scFilter[static_cast<size_t>(ch)].setHighPass(scOn ? scHp->current() : 0.0);
        env[static_cast<size_t>(ch)].setAttackMs(attack->current());
        env[static_cast<size_t>(ch)].setReleaseMs(release->current());
        env[static_cast<size_t>(ch)].setAutoRelease(autoRelease.on());
    }

    // external key: the sidechain bus when it is enabled by the host and carries data; otherwise the internal key
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
        const float t = static_cast<float>(i + 1) / static_cast<float>(n);
        const float mk = mkStart + (mkEnd - mkStart) * t;
        float level[kMaxChannels] = {0.0f, 0.0f};
        float keySample[kMaxChannels] = {0.0f, 0.0f};
        for (int ch = 0; ch < numMain; ++ch)
        {
            float k;
            if (keyChannels > 0)
                k = key[juce::jmin(ch, keyChannels - 1)][i];
            else
                k = feedback ? lastOut[static_cast<size_t>(ch)] : buffer.getSample(ch, i);
            k = scFilter[static_cast<size_t>(ch)].process(k);
            keySample[ch] = k;
            level[ch] = detector[static_cast<size_t>(ch)].process(k);
        }
        const float maxLevel = numMain > 1 ? juce::jmax(level[0], level[1]) : level[0];
        for (int ch = 0; ch < numOut; ++ch)
        {
            const int src = juce::jmin(ch, numMain - 1);
            const float lvl = numMain > 1 ? level[src] + (maxLevel - level[src]) * linkAmt : level[src];
            const float reduction = -gc.gainDb(mm::gainToDb(lvl));
            const float smoothedRed = env[static_cast<size_t>(ch)].process(reduction);
            maxReduction = juce::jmax(maxReduction, smoothedRed);
            const float dry = buffer.getSample(ch, i);
            const float wet = dry * mm::dbToGain(-smoothedRed) * mk;
            const float out = listen ? keySample[src] : (wet * mixAmt + dry * (1.0f - mixAmt));
            lastOut[static_cast<size_t>(ch)] = wet;
            buffer.setSample(ch, i, out);
        }
    }
    reportGainReduction(maxReduction);
}

juce::AudioProcessorEditor* ChannelCompProcessor::createEditor() { return new ChannelCompEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChannelCompProcessor(); }
