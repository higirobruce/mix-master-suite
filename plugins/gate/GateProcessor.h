#pragma once
#include "core/dynamics/EnvelopeFollower.h"
#include "core/dynamics/GainComputer.h"
#include "core/dynamics/Lookahead.h"
#include "core/dynamics/SidechainFilter.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>

// Gate / Expander (ADR-0005): sidechain HP/LP -> peak key (linked: max over channels) -> lookahead sliding max ->
// 2 ms peak hold (so tones read as their amplitude and hysteresis can act) -> state machine with hysteresis and hold
// (gate) or expander gain computer -> attack/release ballistics on the gain.
class GateProcessor : public mm::PluginProcessorBase
{
public:
    GateProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    int dspLatencySamples() const override { return lookahead.latencySamples(); }
    int maxDspLatencySamples() const override { return maxLookaheadSamples; }

    bool isOpen() const noexcept { return openFlag.load(std::memory_order_relaxed); } // editor
    float staticGainDb(float levelDb) noexcept;                                     // editor: expander curve
    static constexpr double kMaxLookaheadMs = 5.0;
    static constexpr double kPeakHoldMs = 2.0; // key sliding-max window: tones >= 250 Hz read flat

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;

private:
    enum class State
    {
        Closed,
        Open,
        Hold
    };
    static constexpr int kMaxChannels = 2;
    ParamRef mode, scHpEnabled, scLpEnabled, extKey, listenSc;
    mm::Smoother<float>*threshold, *range, *attack, *hold, *release, *ratio, *hysteresis, *lookaheadMs, *scHp, *scLp;
    std::array<mm::SidechainFilter<float>, kMaxChannels> scFilter;
    mm::Lookahead<float> lookahead, peakHold;
    mm::EnvelopeFollower<float> env;
    mm::GainComputer<float> gc;
    State state = State::Closed;
    int holdLeft = 0, maxLookaheadSamples = 0;
    std::atomic<bool> openFlag{false};
    double sr = 48000.0;
};
