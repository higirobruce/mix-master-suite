#pragma once
#include "core/delay/IntegerDelay.h"
#include "core/dynamics/Detector.h"
#include "core/dynamics/EnvelopeFollower.h"
#include "core/dynamics/GainComputer.h"
#include "core/dynamics/SidechainFilter.h"
#include "core/oversampling/Oversampler.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>

// Bus / glue compressor (M5): feed-forward VCA model with the stepped ratio / attack / release of the classic bus
// compressors, a stereo-linked peak detector on (|L| + |R|) / 2, a 6 dB knee, the core's program-dependent release
// for Auto, sidechain high-pass, external key, makeup and mix. Optional 2x oversampling runs the whole detector and
// gain chain at twice the rate; the two paths crossfade over 10 ms when the switch is thrown (the oversampled path
// warms up for its latency first) and the reported latency follows at the end of the fade.
class BusCompProcessor : public mm::PluginProcessorBase
{
public:
    static constexpr double kKneeDb = 6.0, kFadeMs = 10.0;
    static constexpr double kRatios[3] = {2.0, 4.0, 10.0};
    static constexpr double kAttacksMs[6] = {0.1, 0.3, 1.0, 3.0, 10.0, 30.0};
    static constexpr double kReleasesMs[4] = {100.0, 300.0, 600.0, 1200.0};

    BusCompProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    double ratioValue() const noexcept;
    double attackMs() const noexcept;
    double releaseMs() const noexcept; // < 0 = Auto
    int oversamplerLatency() const noexcept { return osLatency; }
    float staticGainDb(float levelDb) noexcept; // transfer curve for the editor

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void beginBlock(int numSamples) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    int dspLatencySamples() const override { return reportedLatency; }
    int maxDspLatencySamples() const override { return osLatency; }

private:
    static constexpr int kChannels = 2;
    // one detector / gain chain at a given rate
    struct Chain
    {
        std::array<mm::SidechainFilter<float>, kChannels> sc;
        mm::Detector<float> detector;
        mm::EnvelopeFollower<float> env;
        void prepare(double rate);
        void reset();
        // gain for one frame from the key samples; `red` receives the smoothed reduction in dB
        float gain(float keyL, float keyR, const mm::GainComputer<float>& gc, float& red) noexcept;
    };
    void configure(Chain& c) noexcept; // per block: HP, attack, release, auto
    void runPlain(const float* const* in, const float* const* key, int keyChannels, float* const* out, int n, int numMain, float& maxRed) noexcept;
    void runOversampled(const float* const* in, const float* const* key, int keyChannels, float* const* out, int n, int numMain, float& maxRed) noexcept;

    ParamRef ratioRef, attackRef, releaseRef, oversampleRef, scExternal;
    mm::Smoother<float>*threshold = nullptr, *makeup = nullptr, *mix = nullptr, *scHp = nullptr;
    mm::GainComputer<float> gc;
    Chain plain, over;
    mm::Oversampler os, keyOs;
    mm::IntegerDelay<float> mixDelay; // the dry side of the mix, aligned to the reported latency
    std::array<std::vector<float>, kChannels> wetA, wetB, keyUp, dry;
    double sr = 48000.0;
    int osLatency = 0, reportedLatency = 0, maxBlockSamples = 64;
    // path state: 0 plain, 1 oversampled; a switch warms the oversampled path for its latency, then fades 10 ms
    int activePath = 0, pendingPath = -1, warmupRemaining = 0, fadeRemaining = 0, fadeTotal = 1;
    float lastMakeupGain = 1.0f, lastMix = 1.0f;
    bool rampsValid = false;
};
