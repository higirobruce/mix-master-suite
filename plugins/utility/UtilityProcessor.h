#pragma once
#include "core/filters/AllPass.h"
#include "core/filters/OnePole.h"
#include "plugin_base/PluginProcessorBase.h"

// Utility / Trim: gain, polarity, L/R swap, mono sum, DC block, phase rotation, channel mutes (spec: Mixing plugins).
class UtilityProcessor : public mm::PluginProcessorBase
{
public:
    UtilityProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;

private:
    static constexpr int kMaxChannels = 2;
    ParamRef polarityL, polarityR, swapLR, monoSum, dcBlockOn, muteL, muteR;
    mm::Smoother<float>* gainSmoother = nullptr;
    mm::Smoother<float>* rotationSmoother = nullptr;
    mm::DbGainCache gainCache;
    mm::OnePoleHP<float> dcBlock[kMaxChannels];
    mm::FirstOrderAllPass<float> phaseRot[kMaxChannels];
    bool dcWasOn = false;
    bool phaseWasOn = false;
    double sampleRate = 48000.0;
};
