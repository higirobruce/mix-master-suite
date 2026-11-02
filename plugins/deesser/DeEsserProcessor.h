#pragma once
#include "core/dynamics/EnvelopeFollower.h"
#include "core/dynamics/Lookahead.h"
#include "core/filters/LinkwitzRiley.h"
#include "core/filters/SVF.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>

// De-esser (ADR-0009): band-pass (Band) or LR4 high band (Split) detector -> peak -> lookahead sliding max ->
// reduction = min(range, level - threshold) -> envelope (0.5 / 60 ms) -> dynamic bell (Band) or high-band gain
// (Split). Linked detection, one gain for both channels; listen = dry - wet; lookahead reported as latency.
class DeEsserProcessor : public mm::PluginProcessorBase
{
public:
    DeEsserProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int dspLatencySamples() const override { return lookahead.latencySamples(); }
    int maxDspLatencySamples() const override { return maxLookaheadSamples; }
    bool sibilantNow() const noexcept { return sibilant.load(std::memory_order_relaxed); }
    static constexpr double kMaxLookaheadMs = 3.0, kAttackMs = 0.5, kReleaseMs = 60.0;

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void beginBlock(int numSamples) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;

private:
    static constexpr int kChannels = 2;
    ParamRef mode, listen, lookaheadRaw;
    mm::Smoother<float>*freq, *threshold, *range, *width;
    std::array<mm::SVF<float>, kChannels> detectorBp, bell;
    std::array<mm::LinkwitzRiley<float>, kChannels> splitDet, splitOut; // detector (undelayed) and output (delayed dry)
    mm::Lookahead<float> lookahead;
    mm::EnvelopeFollower<float> env;
    std::array<std::vector<float>, kChannels> dryBuf;
    double sr = 48000.0, tapFade = 1.0, tapFadeStep = 1.0;
    int oldWindow = 0, maxLookaheadSamples = 0;
    unsigned modCounter = 0;
    // mode and listen are switches: both filter sets run during a 10 ms crossfade on a mode change (the incoming
    // set starts from reset), listen fades between wet and the removed part
    bool activeSplit = false;
    double modeFade = 1.0, listenMix = 0.0, switchStep = 1.0;
    static constexpr double kSwitchMs = 10.0;
    double designedFc = -1.0, designedQ = -1.0; // the band the filters were last designed for (per block cache)
    std::atomic<bool> sibilant{false};
};
