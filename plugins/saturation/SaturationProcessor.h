#pragma once
#include "core/filters/Biquad.h"
#include "core/filters/OnePole.h"
#include "core/nonlinear/ADAA.h"
#include "core/nonlinear/ADAATable.h"
#include "core/nonlinear/TubeTransfer.h"
#include "core/nonlinear/Waveshapers.h"
#include "core/oversampling/Oversampler.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <vector>

// Saturation (ADR-0006): fixed 4x oversampling; Tape (emphasis pair + atan/ADAA + head bump), Tube (TubeTransfer),
// Transistor (tanh/ADAA), Clip (soft clip/ADAA); bias shifts the operating point DC-free; unity small-signal gain.
class SaturationProcessor : public mm::PluginProcessorBase
{
public:
    SaturationProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int oversamplerLatency() const noexcept { return oversampler.latencySamples(); }

    static constexpr int kCharacters = 4, kHarmonics = 16;
    // Drive (dB) at which a 1 kHz tone at -12 dBFS produces 1 % THD through the static stage (alias-gate reference).
    static float referenceDriveDb(int character) noexcept;
    // Harmonic levels (dB) of a 1 kHz tone at -12 dBFS through the static stage: GUI thread, no DSP state touched.
    static std::array<float, kHarmonics> harmonicLevelsDb(float driveDb, int character, float bias) noexcept;
    // Static stage transfer for one sample (no ADAA, no filters): shared by the measurements above and the editor.
    static float staticStage(float x, int character, float driveGain, float bias) noexcept;

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    int dspLatencySamples() const override { return oversampler.latencySamples(); }
    int maxDspLatencySamples() const override { return oversampler.latencySamples(); }

private:
    static constexpr int kMaxChannels = 2;
    static float softClipK(float x) noexcept { return mm::shape::softClip(x, 0.5f); }
    static double softClipKAD1D(double x) noexcept { return mm::shape::softClipAD1D(x, 0.5); }
    struct ChannelDsp
    {
        mm::TabulatedADAA1<mm::shape::tanhS, mm::shape::tanhAD1D> tanhStage; // tabulated: two transcendentals per sample
        mm::TabulatedADAA1<mm::shape::atanS, mm::shape::atanAD1D> atanStage; // were ~45 ns per evaluation at 4x
        mm::ADAA1<softClipK, softClipKAD1D> clipStage;
        mm::FirstOrderShelf<float> preEmphasis, deEmphasis, headBump; // host rate (linear: commute with the resamplers)
        mm::OnePoleLP<float> rolloff;
        mm::OnePoleHP<double> dcBlock; // host rate; an asymmetric curve rectifies: any bias turns a zero-mean signal into DC
        void reset() noexcept
        {
            dcBlock.reset();
            tanhStage.reset();
            atanStage.reset();
            clipStage.reset();
            preEmphasis.reset();
            deEmphasis.reset();
            headBump.reset();
            rolloff.reset();
        }
    };
    void designFilters(double osRate, double hostRate);

    ParamRef character;
    mm::Smoother<float>*drive, *bias, *hfRolloff, *mix, *output;
    std::array<ChannelDsp, kMaxChannels> dsp;
    mm::TubeTransfer tube;
    mm::Oversampler oversampler;
    std::array<std::vector<float>, kMaxChannels> dryBuf, dryCarry; // sized in prepareDsp
    double hostRate = 48000.0;
    int lastCharacter = -1;
    float lastRolloff = -1.0f;
};
