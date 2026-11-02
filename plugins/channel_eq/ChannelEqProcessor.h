#pragma once
#include "core/filters/Biquad.h"
#include "core/filters/OnePole.h"
#include "core/filters/SVF.h"
#include "core/metering/Spectrum.h"
#include "core/oversampling/Oversampler.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <complex>

// Channel EQ (ADR-0003): HP/LP 6-48 dB/oct Butterworth cascades, five matched-response bands (bell / shelves),
// analog (proportional) Q, optional 2x oversampling, pre/post spectrum analyser. All filters run in double.
class ChannelEqProcessor : public mm::PluginProcessorBase
{
public:
    ChannelEqProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Response of the running chain at hz from the audio-thread design (tests only: reads DSP state).
    std::complex<double> evaluateAt(double hz) const;
    // Response designed from the current parameter values alone, on the caller's thread (curve display): touches
    // no DSP state, so the GUI never races the audio thread.
    std::complex<double> staticResponseAt(double hz) const;
    void updateDesignForTests() // tests only (single-threaded): design now from the parameter targets
    {
        snapSmoothersToTargets();
        syncMode();
        updateDesign(true);
    }
    mm::SpectrumAnalyser& preAnalyser() noexcept { return analyserPre; }
    mm::SpectrumAnalyser& postAnalyser() noexcept { return analyserPost; }

    struct BandView
    {
        bool enabled;
        int type; // 0 bell, 1 low shelf, 2 high shelf
        double freq, gainDb, q;
    };
    BandView band(int index) const noexcept; // 0..4, current parameter values (valid before prepareToPlay too)

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void beginBlock(int numSamples) override { syncMode(); } // oversampling / Q mode switches change latency: decide first
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    int dspLatencySamples() const override { return oversampleOn ? oversampler.latencySamples() : 0; }
    int maxDspLatencySamples() const override { return maxOsLatency; }

private:
    static constexpr int kBands = 5, kMaxChannels = 2, kMaxFilterSections = 4;
    struct BandRefs
    {
        ParamRef enabled, type, freqRaw, gainRaw, qRaw;
        mm::Smoother<float>* freq = nullptr;
        mm::Smoother<float>* gain = nullptr;
        mm::Smoother<float>* q = nullptr;
        double lastFreq = -1, lastGain = 0, lastQ = -1;
        int lastType = -1;
        bool lastEnabled = false;
        mm::BiquadCoeffs<double> coeffs, prevCoeffs;
        bool active = false, ramping = false;
        double onPos = 0.0, onTarget = 0.0; // switch ramp: effective gain = gain * onPos, 20 ms, phase-continuous
    };
    struct FilterRefs
    {
        ParamRef enabled, slope, freqRaw;
        mm::Smoother<float>* freq = nullptr;
        double lastFreq = -1;
        int lastSlope = -1;
        bool lastEnabled = false;
        bool active = false, useOnePole = false, ramping = false, pendingSlope = false;
        int numBiquads = 0;
        std::array<mm::BiquadCoeffs<double>, kMaxFilterSections> coeffs, prevCoeffs;
        double onePoleRate = 0.0, onePoleFc = 100.0, prevOnePoleFc = 100.0;
        double onPos = 0.0, onTarget = 0.0; // switch ramp: cutoff glides between the band edge (off) and fc (on)
        double glideStep = 1.0;              // per sample: 5 ms per octave of glide, 20 ms minimum (resonant sections)
    };
    double switchStep = 1.0; // onPos change per sample at the design rate (20 ms ramps)
    static constexpr double kSwitchMs = 20.0, kGlideMsPerOctave = 5.0;

    void syncMode();                 // oversampling / analog-Q switches (latency changes): called from beginBlock
    void updateDesign(bool force);
    void designFilter(FilterRefs& f, bool highPass, double designRate, bool direct);
    void designBand(int index, double designRate, bool direct);
    void resetFilterState(bool highPass) noexcept;
    double processSample(double v, int ch) noexcept;
    void applyRamp(double t, size_t sampleIndex) noexcept;

    std::array<BandRefs, kBands> bands;
    FilterRefs hp, lp;
    ParamRef qMode, oversampleRef, analyserRef;

    // per channel state
    std::array<std::array<mm::Biquad<double>, kBands>, kMaxChannels> bandFilters;
    // HP/LP sections are TPT state-variable filters: the same bilinear transfer function as the RBJ design, but
    // state-consistent when the cutoff moves (a TDF-II biquad releases its stored energy as a transient; measured
    // +5.5 dB on a 48 dB/oct glide)
    std::array<std::array<mm::SVF<double>, kMaxFilterSections>, kMaxChannels> hpFilters, lpFilters;
    std::array<mm::OnePoleHP<double>, kMaxChannels> hpOnePole;
    std::array<mm::OnePoleLP<double>, kMaxChannels> lpOnePole;

    mm::Oversampler oversampler;
    mm::SpectrumAnalyser analyserPre, analyserPost;
    double hostRate = 48000.0, designRate = 48000.0;
    int lastBlockSamples = 64; // design-rate samples of the block being designed (switch ramps advance by it)
    bool oversampleOn = false, lastAnalogQ = false;
    int maxOsLatency = 0;
};
