#pragma once
#include "core/delay/IntegerDelay.h"
#include "core/oversampling/Oversampler.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>
#include <vector>

// Soft clipper (M5): input gain -> 4x / 8x oversampling -> first-order ADAA soft clip against the ceiling with a
// quadratic knee (knee 0 = hard) -> down -> mix / delta against the delayed dry. With the true-peak ceiling on, the
// clip runs a measured margin under the ceiling so the down-sampler's overshoot never takes the output's true peak
// above it; with it off the ceiling is exact on the oversampled samples and the output is clamped at the ceiling.
// The oversampling switch warms the incoming path up for its latency, then crossfades 10 ms and re-reports.
class SoftClipperProcessor : public mm::PluginProcessorBase
{
public:
    static constexpr int kChannels = 2, kPaths = 2; // 4x, 8x
    static constexpr double kFadeMs = 10.0;
    // margins (dB) the clip sits under the ceiling with the true-peak option on: the half-band down-sampler's worst
    // overshoot on clipped material (measured on +6 dBFS noise and 0.45 fs bursts at four rates)
    static constexpr float kTruePeakMarginDb[kPaths] = {0.6f, 0.4f};

    SoftClipperProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int oversamplerLatency(int pathIndex) const noexcept { return paths[static_cast<size_t>(pathIndex)].os.latencySamples(); }
    float staticGainDb(float levelDb) noexcept; // the clip curve as gain at an input level (transfer display)
    float currentClipDb() const noexcept { return clipDb.load(std::memory_order_relaxed); }

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void beginBlock(int numSamples) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    int dspLatencySamples() const override { return reportedLatency; }
    int maxDspLatencySamples() const override { return maxLatency; }

private:
    struct Path
    {
        mm::Oversampler os;
        std::array<double, kChannels> x1{}; // ADAA state per channel
        void reset() noexcept
        {
            os.reset();
            x1.fill(0.0);
        }
    };
    // clip the lanes in `out` (n frames, in place) through `path` at the given ceiling / knee ramps
    void runPath(Path& path, float* const* out, int n, float cStart, float cEnd, float kStart, float kEnd, float& maxClip) noexcept;

    mm::Smoother<float>*input = nullptr, *ceiling = nullptr, *knee = nullptr, *mix = nullptr;
    ParamRef oversampleRef, truePeakRef, deltaRef;
    std::array<Path, kPaths> paths;
    mm::IntegerDelay<float> dryDelay;
    std::array<std::vector<float>, kChannels> wetA, wetB, dry;
    std::atomic<float> clipDb{0.0f};
    double sr = 48000.0;
    int reportedLatency = 0, maxLatency = 0;
    int activePath = 0, pendingPath = -1, warmupRemaining = 0, fadeRemaining = 0, fadeTotal = 1;
    bool rampsValid = false;
};
