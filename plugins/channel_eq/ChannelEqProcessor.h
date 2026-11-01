#pragma once
#include "core/filters/Biquad.h"
#include "core/filters/OnePole.h"
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

    // Response of the whole chain at hz (host rate), for the curve display and tests.
    std::complex<double> evaluateAt(double hz) const;
    void updateDesignForTests()
    {
        snapSmoothersToTargets();
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
    BandView band(int index) const noexcept; // 0..4, from the smoothed values last designed

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    int dspLatencySamples() const override { return oversampleOn ? oversampler.latencySamples() : 0; }
    int maxDspLatencySamples() const override { return maxOsLatency; }

private:
    static constexpr int kBands = 5, kMaxChannels = 2, kMaxFilterSections = 4;
    struct BandRefs
    {
        ParamRef enabled, type;
        mm::Smoother<float>* freq = nullptr;
        mm::Smoother<float>* gain = nullptr;
        mm::Smoother<float>* q = nullptr;
        double lastFreq = -1, lastGain = 0, lastQ = -1;
        int lastType = -1;
        bool lastEnabled = false;
        mm::BiquadCoeffs<double> coeffs, prevCoeffs;
        bool active = false, ramping = false;
    };
    struct FilterRefs
    {
        ParamRef enabled, slope;
        mm::Smoother<float>* freq = nullptr;
        double lastFreq = -1;
        int lastSlope = -1;
        bool lastEnabled = false;
        bool active = false, wasActive = false, useOnePole = false, ramping = false;
        int numBiquads = 0;
        std::array<mm::BiquadCoeffs<double>, kMaxFilterSections> coeffs, prevCoeffs;
    };

    void updateDesign(bool force);
    void designFilter(FilterRefs& f, bool highPass, double designRate);
    void designBand(BandRefs& b, double designRate);
    double processSample(double v, int ch) noexcept;
    void applyRamp(double t) noexcept;

    std::array<BandRefs, kBands> bands;
    FilterRefs hp, lp;
    ParamRef qMode, oversampleRef, analyserRef;

    // per channel state
    std::array<std::array<mm::Biquad<double>, kBands>, kMaxChannels> bandFilters;
    std::array<std::array<mm::Biquad<double>, kMaxFilterSections>, kMaxChannels> hpFilters, lpFilters;
    std::array<mm::OnePoleHP<double>, kMaxChannels> hpOnePole;
    std::array<mm::OnePoleLP<double>, kMaxChannels> lpOnePole;

    mm::Oversampler oversampler;
    mm::SpectrumAnalyser analyserPre, analyserPost;
    double hostRate = 48000.0, designRate = 48000.0;
    bool oversampleOn = false, lastAnalogQ = false;
    int maxOsLatency = 0;
};
