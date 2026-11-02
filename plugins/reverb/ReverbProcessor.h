#pragma once
#include "core/delay/IntegerDelay.h"
#include "core/filters/SVF.h"
#include "core/reverb/DattorroPlate.h"
#include "core/reverb/EarlyReflections.h"
#include "core/reverb/FDN.h"
#include "plugin_base/PluginProcessorBase.h"
#include <array>

// Reverb (ADR-0008): pre-delay -> early reflections + late engine (two FDN-16 instances for Room/Hall/Chamber so an
// algorithm or size change crossfades between them over 50 ms; Dattorro plate for Plate) -> early/late balance ->
// low/high cut -> width -> mix. Zero latency.
class ReverbProcessor : public mm::PluginProcessorBase
{
public:
    ReverbProcessor();
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    static constexpr double kFadeMs = 200.0, kMaxPredelayMs = 250.0; // algorithm/size switches morph over 200 ms (the incoming engine starts empty)

protected:
    void prepareDsp(double sampleRate, int maxBlock, int channels) override;
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void resetDsp() override;
    bool producesStereoFromMono() const override { return true; }

private:
    enum Engine
    {
        FdnA = 0,
        FdnB = 1,
        Plate = 2
    };
    void configureFdn(mm::reverb::FDN<float>& f, int algorithm, double size);
    void switchTo(int algorithm, double size);
    void engineProcess(int engine, float inL, float inR, float& outL, float& outR) noexcept;

    ParamRef algorithm, predelay, size;
    mm::Smoother<float>*decay, *damping, *diffusion, *lowCut, *highCut, *earlyLate, *mix, *width;
    std::array<mm::reverb::FDN<float>, 2> fdn;
    mm::reverb::DattorroPlate<float> plate;
    mm::reverb::EarlyReflections<float> early;
    mm::IntegerDelay<float> predelayLine;
    std::array<mm::SVF<float>, 2> lowCutF, highCutF;
    int cur = FdnA, prev = -1, lastAlgorithm = -1;
    double lastSize = -1.0, fadePos = 1.0, fadeStep = 1.0, sr = 48000.0;
    int lastPredelay = -1;
};
