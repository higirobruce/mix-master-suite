#pragma once
#include "core/common/Dither.h"
#include "core/dynamics/Lookahead.h"
#include "core/metering/LoudnessMeter.h"
#include "core/metering/TruePeakMeter.h"
#include "core/oversampling/Oversampler.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>
#include <vector>

// True-peak limiter (ADR-0012): input gain -> 4x oversampling at every rate -> per-channel true-peak level, linked
// by link_pct -> required gain against the ceiling -> held at its minimum over the lookahead window (sliding max of
// the reduction) and averaged over the window (the attack is the lookahead: the gain reaches the held value exactly
// when the peak arrives) -> dual-stage release (a slow stage with its own attack, a fast stage covering the rest
// instantly) -> gain on the audio delayed by the window -> down -> TPDF dither. Output true-peak and loudness meters
// for the editor; lookahead changes are integer latency steps with the core lookahead's tap crossfade.
class LimiterProcessor : public mm::PluginProcessorBase
{
public:
    static constexpr double kMaxLookaheadMs = 10.0;
    static constexpr double kMinLookaheadMs = 0.1; // the attack ramp's floor: a zero-width ramp is a clipper whose reconstruction overshoots (ADR-0012)
    static constexpr int kChannels = 2;
    struct Character
    {
        double fastMs, slowMs;
    };
    static constexpr Character kCharacters[3] = {{30.0, 300.0}, {10.0, 500.0}, {50.0, 150.0}};

    LimiterProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int oversamplerLatency() const noexcept { return os.latencySamples(); }
    float outputTruePeakDbtp() const noexcept { return outTp.load(std::memory_order_relaxed); }
    float integratedLufs() const noexcept { return outLufs.load(std::memory_order_relaxed); }
    float loudnessRangeLu() const noexcept { return outLra.load(std::memory_order_relaxed); }
    bool overNow() const noexcept { return over.load(std::memory_order_relaxed); }
    void resetMeters() noexcept { metersReset.store(true, std::memory_order_relaxed); } // editor: restart the integration

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void beginBlock(int numSamples) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    int dspLatencySamples() const override { return lookaheadHost + os.latencySamples(); }
    int maxDspLatencySamples() const override { return maxLookaheadHost + os.latencySamples(); }

private:
    struct ChannelState
    {
        mm::Lookahead<float> hold;         // sliding max of the reduction (dB) over the window
        std::vector<float> avgRing;        // the held values over the window, for the moving average
        int avgPos = 0, avgCount = 0;
        double avgSum = 0.0;
        double slowDb = 0.0, fastDb = 0.0; // the two release stages (reduction in dB, >= 0)
        void reset() noexcept
        {
            hold.reset();
            std::fill(avgRing.begin(), avgRing.end(), 0.0f);
            avgPos = avgCount = 0;
            avgSum = 0.0;
            slowDb = fastDb = 0.0;
        }
    };
    float movingAverage(ChannelState& c, float held) noexcept;

    mm::Smoother<float>*input = nullptr, *ceiling = nullptr, *release = nullptr, *link = nullptr;
    ParamRef autoRelease, lookaheadRaw, characterRef, ditherRef;
    mm::Oversampler os;
    mm::Lookahead<float> delay; // the audio delayed by the window at the oversampled rate
    std::array<ChannelState, kChannels> chans;
    std::array<mm::Dither, kChannels> dither;
    mm::TruePeakMeter tpMeter;
    mm::LoudnessMeter lmMeter;
    std::atomic<float> outTp{-200.0f}, outLufs{-120.0f}, outLra{0.0f};
    std::atomic<bool> over{false}, metersReset{false};
    double sr = 48000.0, osRate = 192000.0;
    int factor = 4, lookaheadHost = 0, minLookaheadHost = 1, maxLookaheadHost = 0, windowOs = 0, oldWindowOs = 0, maxWindowOs = 1;
    double tapFade = 1.0, tapFadeStep = 1.0;
    // crest factor over the last 50 ms for Auto: peak and mean square accumulators
    double crestPeak = 0.0, crestMs = 0.0;
    int crestCount = 0, crestWindow = 2400;
    double crestDb = 12.0;
};
