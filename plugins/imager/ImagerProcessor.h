#pragma once
#include "core/filters/Biquad.h"
#include "core/filters/LinkwitzRiley.h"
#include "core/metering/Correlation.h"
#include "core/metering/Spectrum.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>
#include <vector>

// Stereo imager (ADR-0010): M/S encode -> S through two LR4 crossovers (three bands, the low band all-passed
// through the upper crossover) scaled by width, M through the same two all-passes (so M and S stay phase-aligned) -> mono-below (LR4 on S, off at 0) -> decode ->
// constant-power pan -> output. L + R never changes with width. Zero latency.
class ImagerProcessor : public mm::PluginProcessorBase
{
public:
    ImagerProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    float currentCorrelation() const noexcept { return correlationValue.load(std::memory_order_relaxed); }
    // the last stereo output samples for the goniometer (GUI thread; returns how many were copied, newest last)
    int pullScope(float* l, float* r, int maxSamples) const noexcept;
    static constexpr int kScopeSize = 4096;
    mm::SpectrumAnalyser& outputAnalyser() noexcept { return analyserOut; }

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    bool producesStereoFromMono() const override { return true; }

private:
    mm::Smoother<float>*xoverLow, *xoverHigh, *widthLow, *widthMid, *widthHigh, *pan, *output;
    ParamRef monoBelow; // unsmoothed: engaged by a 20 ms crossfade, then glided in the log domain (ADR-0010)
    // S: sLow splits, sHigh splits the upper part, sLowAp (at the upper crossover) all-passes the low band so the
    // three-band sum equals the two-stage all-pass; M: the same two all-passes. An LR4's low + high is exactly the
    // second-order all-pass with its cutoff and Q = 1/sqrt(2), so every all-pass leg is one biquad. 64-bit: the
    // all-pass nulls to < -100 dBFS at 192 k.
    mm::LinkwitzRiley<double> sLow, sHigh, sMono;
    mm::Biquad<double> sLowAp, mAp1, mAp2, mMonoAp;
    double apLowHz = 0.0, apHighHz = 0.0; // cutoffs the all-pass coefficients were designed for
    mm::CorrelationMeter correlation;
    mm::SpectrumAnalyser analyserOut;
    std::atomic<float> correlationValue{1.0f};
    std::vector<float> scopeL, scopeR;
    std::atomic<int> scopeWrite{0};
    bool monoActive = false;          // the mono-below filters run (engaged, or fading out)
    double monoFc = 0.0, monoMix = 0.0; // glided cutoff; engagement crossfade 0..1
    double monoMixStep = 1.0, monoGlideOctPerSample = 1.0;
    static constexpr double kMonoFadeMs = 20.0, kMonoGlideMsPerOctave = 5.0, kMonoMinHz = 5.0;
    double sr = 48000.0;
};
