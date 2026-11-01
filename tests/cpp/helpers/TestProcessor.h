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
    TestProcessor()
        : mm::PluginProcessorBase(testTable(), "test_plugin",
                                  BusesProperties()
                                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    std::atomic<bool> injectNaN{false};
    static constexpr int kLatency = 10;

protected:
    void prepareDsp(double, int maxBlock, int channels) override
    {
        delay.prepare(kLatency, channels);
        delay.setDelay(kLatency);
        (void) maxBlock;
    }
    void resetDsp() override { delay.reset(); }
    int dspLatencySamples() const override { return kLatency; }
    void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        auto& g = smoothed(ids::gain_db);
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
};
} // namespace mm::test
