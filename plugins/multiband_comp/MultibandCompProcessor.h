#pragma once
#include "core/dynamics/Detector.h"
#include "core/dynamics/EnvelopeFollower.h"
#include "core/dynamics/GainComputer.h"
#include "core/filters/Biquad.h"
#include "core/filters/LinkwitzRiley.h"
#include "core/metering/Spectrum.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>

// Multiband compressor (M5): 3-5 bands from a tree of 64-bit LR4 crossovers (each band passes through the
// all-passes of the later crossovers, so the bands sum to the all-pass of every crossover and the chain is
// phase-coherent at 1:1); per band a linked detector (peak or 10 ms RMS over the channels' maximum), a 6 dB soft-knee
// gain computer, an envelope on the reduction, a gain, solo and bypass; global mix against the all-passed dry sum;
// optional external key through its own crossover tree. Crossovers are kept a third of an octave apart. Zero latency.
class MultibandCompProcessor : public mm::PluginProcessorBase
{
public:
    static constexpr int kMaxBands = 5, kMaxCrossovers = 4, kChannels = 2;
    static constexpr double kKneeDb = 6.0, kRmsMs = 10.0, kMinRatioOct = 1.0 / 3.0, kSwitchMs = 20.0;
    static constexpr double kWarmMs = 30.0; // a band-count change: the new tree warms up silently, then crossfades over kSwitchMs

    MultibandCompProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    int numBands() const noexcept;                   // 3..5 from the parameter
    float bandReductionDb(int band) const noexcept { return reduction[static_cast<size_t>(band)].load(std::memory_order_relaxed); }
    float bandLevelDb(int band) const noexcept { return levelDb[static_cast<size_t>(band)].load(std::memory_order_relaxed); }
    double crossoverHz(int index) const noexcept { return xoverInForce[static_cast<size_t>(index)].load(std::memory_order_relaxed); }
    mm::SpectrumAnalyser& postAnalyser() noexcept { return analyser; }

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;

private:
    struct BandRefs
    {
        ParamRef solo, bypass;
        mm::Smoother<float>*threshold = nullptr, *ratio = nullptr, *attack = nullptr, *release = nullptr, *gain = nullptr;
        mm::Detector<float> detector;
        mm::GainComputer<float> gc;
        mm::EnvelopeFollower<float> env;
        double activePos = 1.0, activeTarget = 1.0; // 0 = bypassed (dry band), 1 = compressed; 20 ms ramp
        double audiblePos = 1.0, audibleTarget = 1.0; // solo logic: 0 = muted
    };
    // one crossover tree: LR4 per crossover plus, per band, the all-pass legs of the later crossovers
    struct Tree
    {
        std::array<mm::LinkwitzRiley<double>, kMaxCrossovers> split;
        std::array<std::array<mm::Biquad<double>, kMaxCrossovers>, kMaxBands> ap; // ap[band][crossover]
        void reset() noexcept;
    };
    void designCrossovers(bool force) noexcept;
    void splitBands(Tree& t, double x, double* bands, int n) noexcept; // bands[0..n) phase-aligned

    std::array<BandRefs, kMaxBands> bands;
    std::array<mm::Smoother<float>*, kMaxCrossovers> xover{};
    ParamRef numBandsRef, detectorRef, scExternal;
    mm::Smoother<float>* mix = nullptr;
    std::array<Tree, kChannels> trees, keyTrees;
    std::array<Tree, kChannels> fadeTrees; // the outgoing band count's tree during a band-count crossfade
    int fadeN = 0, fadeWarm = 0, warmSamples = 0;
    double fadePos = 0.0, fadeStep = 1.0;
    std::array<double, kMaxCrossovers> designedHz{};
    std::array<mm::BiquadCoeffs<double>, kMaxCrossovers> apCoeffs;
    std::array<std::atomic<float>, kMaxBands> reduction{}, levelDb{};
    std::array<std::atomic<double>, kMaxCrossovers> xoverInForce{};
    mm::SpectrumAnalyser analyser;
    double sr = 48000.0, switchStep = 1.0;
    int lastNumBands = 4;
    unsigned modCounter = 0;
};
