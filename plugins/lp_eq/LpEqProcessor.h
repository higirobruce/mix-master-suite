#pragma once
#include "core/filters/Biquad.h"
#include "core/fir/PartitionedConvolver.h"
#include "core/metering/Spectrum.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>
#include <complex>
#include <memory>
#include <vector>

// Linear-phase EQ (ADR-0011): eight bands, each Minimum (a matched biquad, as the Channel EQ) or Linear (composed
// into one FIR per lane), each on Stereo / Mid / Side. The audio always runs as two lanes, M and S: a Stereo band is
// applied to both lanes, which is exactly L/R processing for identical linear filters, so there is no mode switch.
// Linear impulses are designed by a worker thread into a ring of four slots and swapped in at block start with a
// 20 ms crossfade inside the convolver; the FIR runs even with no linear band (a centred delta), so the latency is
// P + (L - 2) / 2 for the chosen length and nothing else.
class LpEqProcessor : public mm::PluginProcessorBase, private juce::Thread
{
public:
    static constexpr int kBands = 8;
    static constexpr int kMinPartition = 512; // ADR-0011: 16384 taps at P = 64 would cost ~3x the budget
    static constexpr double kFadeMs = 20.0, kDesignHoldMs = 50.0;
    static constexpr int kLengths[4] = {2048, 4096, 8192, 16384};

    LpEqProcessor();
    ~LpEqProcessor() override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    struct BandView
    {
        bool enabled;
        int type, phase, ms; // type 0 bell, 1 low shelf, 2 high shelf, 3 low cut, 4 high cut, 5 notch; phase 0 min, 1 linear; ms 0 stereo, 1 mid, 2 side
        double freq, gainDb, q;
    };
    BandView band(int index) const noexcept; // current parameter values
    // Magnitude (and, for minimum-phase bands, phase) of the enabled bands from the parameter values alone: no DSP
    // state, safe on the message thread. Linear bands contribute their magnitude only.
    std::complex<double> staticResponseAt(double hz) const;
    int firLength() const noexcept;      // 2048 .. 16384 from fir_length
    double resolutionHz() const noexcept; // 2 fs / L
    void redesignForTests();              // synchronous design + swap (single-threaded tests)
    mm::SpectrumAnalyser& preAnalyser() noexcept { return analyserPre; }
    mm::SpectrumAnalyser& postAnalyser() noexcept { return analyserPost; }

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void beginBlock(int numSamples) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    int dspLatencySamples() const override { return convolver.latencySamples(); }
    int maxDspLatencySamples() const override { return maxLatency; }

private:
    static constexpr int kLanes = 2; // 0 mid, 1 side
    struct BandRefs
    {
        ParamRef enabled, type, phase, ms, freqRaw, gainRaw, qRaw;
        mm::Smoother<float>*freq = nullptr, *gain = nullptr, *q = nullptr;
        mm::BiquadCoeffs<double> coeffs;
        double lastFreq = -1.0, lastGain = 1e9, lastQ = -1.0;
        int lastType = -1;
    };
    struct LaneBand
    {
        mm::Biquad<double> filter;
        double onPos = 0.0, onTarget = 0.0; // dry/wet position of the band in this lane (20 ms ramp)
    };
    // what the worker designs from: the targets of the linear bands and the length
    struct DesignRequest
    {
        struct Band
        {
            bool enabled = false;
            int type = 0, ms = 0;
            double freq = 1000.0, gainDb = 0.0, q = 1.0;
            bool operator==(const Band&) const = default;
        };
        std::array<Band, kBands> bands{};
        int length = 4096;
        double sampleRate = 48000.0;
        bool operator==(const DesignRequest&) const = default;
    };
    struct Slot
    {
        std::array<mm::fir::PartitionedConvolver::Spectra, kLanes> spectra;
        std::atomic<int> state{0}; // 0 free, 1 writing, 2 ready, 3 in use (active or fading out)
    };

    void run() override; // worker: designs requests into free slots
    DesignRequest snapshotRequest() const noexcept;
    void publishRequest(const DesignRequest& r) noexcept;
    bool readRequest(DesignRequest& r) const noexcept;
    void designInto(const DesignRequest& r, Slot& slot, std::vector<double>& scratchRe, std::vector<double>& scratchIm);
    void takeReadySlot() noexcept; // audio thread: swap in a ready slot with the crossfade
    void designMinimumPhase(int band) noexcept;
    static mm::BiquadCoeffs<double> coeffsFor(int type, double sr, double freq, double gainDb, double q) noexcept;

    std::array<BandRefs, kBands> bands;
    std::array<std::array<LaneBand, kBands>, kLanes> laneBands;
    ParamRef firLengthRef;
    mm::fir::PartitionedConvolver convolver;
    std::array<Slot, 4> slots;
    int activeSlot = -1, fadingSlot = -1;
    std::array<std::vector<float>, kLanes> laneBuf;
    mm::SpectrumAnalyser analyserPre, analyserPost;
    double sr = 48000.0, switchStep = 1.0;
    int maxLatency = 0, fadeSamples = 960, designCounter = 0;
    unsigned modCounter = 0;

    // request seqlock (audio thread writes, worker reads)
    DesignRequest request;
    DesignRequest lastRequested;
    mutable std::atomic<unsigned> requestSeq{0};
    std::atomic<bool> prepared{false};
};
