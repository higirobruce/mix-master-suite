#include "plugins/utility/UtilityProcessor.h"
#include "Params.h"
#include "plugins/utility/UtilityEditor.h"
#include "utility_data.h"
#include <cmath>
#include <numbers>

namespace up = mm::plugins::utility;

static mm::PresetManager::FactorySource utilityFactory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &utility_data::getNamedResource;
    f.names = utility_data::namedResourceList;
    f.count = utility_data::namedResourceListSize;
    return f;
}

UtilityProcessor::UtilityProcessor()
    : mm::PluginProcessorBase(up::table(), up::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              utilityFactory())
{
    polarityL = paramRef(up::id::polarity_l);
    polarityR = paramRef(up::id::polarity_r);
    swapLR = paramRef(up::id::swap_lr);
    monoSum = paramRef(up::id::mono_sum);
    dcBlockOn = paramRef(up::id::dc_block);
    muteL = paramRef(up::id::mute_l);
    muteR = paramRef(up::id::mute_r);
    gainSmoother = &smoothed(up::id::gain_db);
    rotationSmoother = &smoothed(up::id::phase_rotation_deg);
}

void UtilityProcessor::prepareDsp(double sr, int, int)
{
    sampleRate = sr;
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcBlock[ch].prepare(sr);
        dcBlock[ch].setCutoff(10.0);
        phaseRot[ch].prepare(sr);
    }
    resetDsp();
}

void UtilityProcessor::resetDsp()
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcBlock[ch].reset();
        phaseRot[ch].reset();
    }
    dcWasOn = phaseWasOn = false;
}

void UtilityProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), kMaxChannels);
    const bool stereo = numCh == 2;

    // 1. gain (smoothed in dB, ramped linearly across the block) + polarity
    auto& gain = *gainSmoother;
    const float g0 = gainCache.get(gain.current());
    gain.skip(numSamples);
    const float g1 = gainCache.get(gain.current());
    const float pol[2] = {polarityL.on() ? -1.0f : 1.0f, polarityR.on() ? -1.0f : 1.0f};
    for (int ch = 0; ch < numCh; ++ch)
        if (g0 != 1.0f || g1 != 1.0f || pol[ch] != 1.0f)
            buffer.applyGainRamp(ch, 0, numSamples, g0 * pol[ch], g1 * pol[ch]);

    // 2. swap, 3. mono sum (stereo only)
    if (stereo)
    {
        if (swapLR.on())
        {
            float* l = buffer.getWritePointer(0);
            float* r = buffer.getWritePointer(1);
            for (int n = 0; n < numSamples; ++n)
                std::swap(l[n], r[n]);
        }
        if (monoSum.on())
        {
            float* l = buffer.getWritePointer(0);
            float* r = buffer.getWritePointer(1);
            for (int n = 0; n < numSamples; ++n)
                l[n] = r[n] = 0.5f * (l[n] + r[n]);
        }
    }

    // 4. DC block (stage bypassed exactly when off; filters reset on enable)
    const bool dcOn = dcBlockOn.on();
    if (dcOn)
    {
        if (!dcWasOn)
            for (int ch = 0; ch < kMaxChannels; ++ch)
                dcBlock[ch].reset();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer(ch);
            for (int n = 0; n < numSamples; ++n)
                d[n] = dcBlock[ch].process(d[n]);
        }
    }
    dcWasOn = dcOn;

    // 5. phase rotation: first-order all-pass whose corner puts -deg of phase at 100 Hz
    auto& rot = *rotationSmoother;
    const float degNow = rot.current();
    rot.skip(numSamples);
    const float deg = 0.5f * (degNow + rot.current()); // block-rate update of the corner
    const bool phaseOn = deg >= 0.5f;
    if (phaseOn)
    {
        if (!phaseWasOn)
            for (int ch = 0; ch < kMaxChannels; ++ch)
                phaseRot[ch].reset();
        const double corner = 100.0 / std::tan(static_cast<double>(deg) * std::numbers::pi / 360.0);
        for (int ch = 0; ch < numCh; ++ch)
        {
            phaseRot[ch].setCorner(juce::jlimit(1.0, 0.49 * sampleRate, corner));
            float* d = buffer.getWritePointer(ch);
            for (int n = 0; n < numSamples; ++n)
                d[n] = phaseRot[ch].process(d[n]);
        }
    }
    phaseWasOn = phaseOn;

    // 6. mutes
    if (muteL.on())
        buffer.clear(0, 0, numSamples);
    if (stereo && muteR.on())
        buffer.clear(1, 0, numSamples);
}

juce::AudioProcessorEditor* UtilityProcessor::createEditor() { return new UtilityEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UtilityProcessor(); }
