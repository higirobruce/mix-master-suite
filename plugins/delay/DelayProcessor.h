#pragma once
#include "core/delay/FractionalDelayLine.h"
#include "core/dynamics/EnvelopeFollower.h"
#include "core/filters/SVF.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>

// Delay (ADR-0007): two fractional lines (Lagrange-3), tempo sync from the host play head, feedback through SVF
// LP/HP and a soft limiter (+6 dBFS ceiling), ping-pong, LFO modulation, dry-keyed ducking. Time changes: small
// moves slew (pitch glide), large jumps crossfade between the old and new taps over 20 ms.
class DelayProcessor : public mm::PluginProcessorBase
{
public:
    DelayProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double currentBpm() const noexcept { return bpm.load(std::memory_order_relaxed); }
    // effective delay times (ms) after sync/link, for the editor
    float effectiveTimeMs(int channel) const noexcept { return effectiveMs[static_cast<size_t>(juce::jlimit(0, 1, channel))].load(std::memory_order_relaxed); }
    static constexpr double kMaxDelaySeconds = 4.0, kSlewMs = 20.0, kCrossfadeMs = 20.0, kGlideLimitMs = 50.0;

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void beginBlock(int numSamples) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    bool producesStereoFromMono() const override { return true; }

private:
    static constexpr int kChannels = 2;
    struct TimeState
    {
        double current = 48.0, target = 48.0, increment = 0.0; // samples; small changes slew
        double xfOld = 0.0, xfPos = 1.0;                       // crossfade from xfOld to current (1 = done)
        double pending = -1.0;                                 // a jump requested during a crossfade waits for it
    };
    void setTargetSamples(int ch, double samples) noexcept;

    ParamRef timeL, timeR, linkLr, sync, syncR, pingPong;
    mm::Smoother<float>*feedback, *fbLp, *fbHp, *modDepth, *modRate, *mix, *duckAmount, *duckRelease;
    mm::FractionalDelayLine<float> line;
    std::array<mm::SVF<float>, kChannels> lp, hp;
    std::array<TimeState, kChannels> time;
    std::array<float, kChannels> lastWet{};
    mm::EnvelopeFollower<float> duckEnv;
    double sr = 48000.0, lfoPhase = 0.0, xfStep = 1.0;
    std::atomic<double> bpm{120.0};
    std::array<std::atomic<float>, kChannels> effectiveMs{};
};
