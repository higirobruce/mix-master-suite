#pragma once
// A minimal plugin on the shared base used by the plugin_base tests: gain x2 with 10 samples of latency.
#include "plugin_base/PluginProcessorBase.h"
#include <array>
#include <atomic>

namespace mm::test
{
inline constexpr std::array<mm::ParamSpec, 3> kTestSpecs = {{
    mm::ParamSpec{"gain_db", "Gain", mm::ParamType::Float, "dB", -24.0f, 24.0f, 6.0206f, 1.0f, 20.0f, 0, nullptr, "main", true},
    mm::ParamSpec{"bypass", "Bypass", mm::ParamType::Bool, "", 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0, nullptr, "header", false},
    mm::ParamSpec{"trim_db", "Output trim", mm::ParamType::Float, "dB", -12.0f, 12.0f, 0.0f, 1.0f, 20.0f, 0, nullptr, "footer", false},
}};

inline const mm::ParamTable& testTable()
{
    static const mm::ParamTable t(kTestSpecs);
    return t;
}

class TestProcessor : public mm::PluginProcessorBase
{
public:
    // latency: DSP latency in samples (0 = the common zero-latency case; 10 = the alignment case).
    explicit TestProcessor(int latencySamples = 10)
        : mm::PluginProcessorBase(testTable(), "test_plugin",
                                  BusesProperties()
                                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withInput("Sidechain", juce::AudioChannelSet::mono(), false)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          latency(latencySamples)
    {
    }

    std::atomic<bool> injectNaN{false};
    std::atomic<int> runtimeLatency{-1}; // when >= 0, dspLatencySamples() reports this (runtime latency change)
    static constexpr int kLatency = 10;
    static constexpr int kMaxLatency = 64;

protected:
    void prepareDsp(double, int, int channels) override
    {
        delay.prepare(kMaxLatency, channels);
        delay.setDelay(dspLatencySamples());
    }
    void resetDsp() override { delay.reset(); }
    int dspLatencySamples() const override { return runtimeLatency.load() >= 0 ? runtimeLatency.load() : latency; }
    int maxDspLatencySamples() const override { return kMaxLatency; }
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        auto& g = smoothed(ids::gain_db);
        if (delay.getDelay() != dspLatencySamples())
            delay.setDelay(dspLatencySamples());
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float gain = mm::dbToGain(g.next());
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample(ch, n, delay.process(buffer.getSample(ch, n) * gain, ch));
        }
        if (injectNaN.load())
            buffer.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

private:
    struct ids
    {
        static constexpr const char* gain_db = "gain_db";
    };
    mm::IntegerDelay<float> delay;
    int latency;
};
} // namespace mm::test
