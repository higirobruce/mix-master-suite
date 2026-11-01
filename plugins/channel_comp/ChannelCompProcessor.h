#pragma once
#include "core/dynamics/Detector.h"
#include "core/dynamics/EnvelopeFollower.h"
#include "core/dynamics/GainComputer.h"
#include "core/dynamics/SidechainFilter.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>

// Channel Compressor (ADR-0004): sidechain filter -> peak/RMS detector -> static gain computer -> ballistics on the
// gain reduction; feed-forward or feedback; stereo link; mix; makeup; external key with silent fallback.
class ChannelCompProcessor : public mm::PluginProcessorBase
{
public:
    ChannelCompProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    // transfer curve for the editor (static, current parameters)
    float staticGainDb(float levelDb) const noexcept { return gc.gainDb(levelDb); }

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;

private:
    static constexpr int kMaxChannels = 2;
    ParamRef ratioInf, autoRelease, detectorMode, topology, scHpEnabled, extKey, autoMakeup, listenSc;
    mm::Smoother<float>*threshold, *ratio, *attack, *release, *knee, *mix, *scHp, *makeup, *link;
    std::array<mm::SidechainFilter<float>, kMaxChannels> scFilter;
    std::array<mm::Detector<float>, kMaxChannels> detector;
    std::array<mm::EnvelopeFollower<float>, kMaxChannels> env;
    mm::GainComputer<float> gc;
    std::array<float, kMaxChannels> lastOut{};
    mm::DbGainCache makeupGain;
    double sr = 48000.0;
};
