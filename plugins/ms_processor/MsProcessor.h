#pragma once
#include "core/filters/Biquad.h"
#include "core/filters/LinkwitzRiley.h"
#include "core/metering/Correlation.h"
#include "core/metering/Spectrum.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>
#include <complex>
#include <vector>

// M/S processor (M5): M/S encode (the imager's convention, ADR-0010) -> mid and side gains -> two matched-biquad EQ
// bands per channel (64-bit) -> width on the side -> mono-below (LR4 on the side with the matching all-pass on the
// mid, engaged by a 20 ms crossfade and glided at 5 ms per octave) -> solo -> decode -> output. L + R never changes
// with the width. Zero latency. Correlation meter and a stereo scope ring for the editor's goniometer.
class MsProcessor : public mm::PluginProcessorBase
{
public:
    static constexpr int kScopeSize = 4096, kBands = 2;
    MsProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    float currentCorrelation() const noexcept { return correlationValue.load(std::memory_order_relaxed); }
    int pullScope(float* l, float* r, int maxSamples) const noexcept;
    mm::SpectrumAnalyser& outputAnalyser() noexcept { return analyserOut; }
    // the lane's EQ response from the parameter values alone (lane 0 mid, 1 side; message thread safe)
    std::complex<double> staticResponseAt(int lane, double hz) const;

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    bool producesStereoFromMono() const override { return true; }

private:
    struct Band
    {
        ParamRef enabled, type, freqRaw, gainRaw, qRaw;
        mm::Smoother<float>*freq = nullptr, *gain = nullptr, *q = nullptr;
        mm::Biquad<double> filter;
        double lastFreq = -1.0, lastGain = 1e9, lastQ = -1.0;
        int lastType = -1;
        double onPos = 0.0, onTarget = 0.0; // dry/wet ramp (20 ms)
    };
    static mm::BiquadCoeffs<double> coeffsFor(int type, double rate, double freq, double gainDb, double q) noexcept;
    void designBand(Band& b) noexcept;

    std::array<std::array<Band, kBands>, 2> lanes; // [mid, side][band]
    mm::Smoother<float>*midDb = nullptr, *sideDb = nullptr, *width = nullptr, *output = nullptr;
    ParamRef monoBelow, soloRef;
    mm::LinkwitzRiley<double> sMono;
    mm::Biquad<double> mMonoAp;
    bool monoActive = false;
    double monoFc = 0.0, monoMix = 0.0, monoMixStep = 1.0, monoGlideOctPerSample = 1.0;
    double soloMid = 1.0, soloSide = 1.0; // audibility of each lane (20 ms ramps)
    double switchStep = 1.0;
    static constexpr double kMonoFadeMs = 20.0, kMonoGlideMsPerOctave = 5.0, kMonoMinHz = 5.0, kSwitchMs = 20.0;
    mm::CorrelationMeter correlation;
    mm::SpectrumAnalyser analyserOut;
    std::atomic<float> correlationValue{1.0f};
    std::vector<float> scopeL, scopeR;
    std::atomic<int> scopeWrite{0};
    double sr = 48000.0;
    unsigned modCounter = 0;
};
