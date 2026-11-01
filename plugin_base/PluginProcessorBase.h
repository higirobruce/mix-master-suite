#pragma once
#include "core/common/Math.h"
#include "core/delay/IntegerDelay.h"
#include "core/metering/MeterFifo.h"
#include "core/metering/MeterFrame.h"
#include "core/metering/PeakMeter.h"
#include "core/metering/RmsMeter.h"
#include "core/smoothing/Smoother.h"
#include "plugin_base/AbcdSlots.h"
#include "plugin_base/ParamTable.h"
#include "plugin_base/PresetManager.h"
#include <atomic>
#include <memory>
#include <vector>

namespace mm
{
// Shared processor base (ADR-0002). Subclasses implement prepareDsp/processDsp/resetDsp and read parameters through
// param()/smoothed(). The base owns: APVTS built from the ParamTable, smoothing, gain-matched bypass, delta listen,
// output trim, latency reporting, metering FIFO, A/B/C/D slots, undo, presets and state save/load.
class PluginProcessorBase : public juce::AudioProcessor
{
public:
    PluginProcessorBase(const ParamTable& table, const juce::String& pluginId, BusesProperties buses,
                        PresetManager::FactorySource factory = PresetManager::FactorySource());
    ~PluginProcessorBase() override;

    // ---- host interface (final: the base owns the block structure)
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) final;
    void releaseResources() final;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) final;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) final;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void getStateInformation(juce::MemoryBlock&) final;
    void setStateInformation(const void*, int) final;
    const juce::String getName() const override { return pluginId; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override { return state.getParameter("bypass"); }

    // ---- services for editors and tools
    juce::AudioProcessorValueTreeState& apvts() noexcept { return state; }
    const ParamTable& table() const noexcept { return paramTable; }
    PresetManager& presets() noexcept { return *presetManager; }
    AbcdSlots& slots() noexcept { return *abcd; }
    juce::UndoManager& undo() noexcept { return undoManager; }
    MeterFifo<MeterFrame, 64>& meterFifo() noexcept { return meters; }
    void setDeltaListen(bool on) noexcept { delta.store(on); }
    bool deltaListen() const noexcept { return delta.load(); }
    std::atomic<int>& nanCounter() noexcept { return nanCount; }
    juce::ValueTree& uiState() noexcept { return ui; } // scale, theme, drawer; saved with the plugin state
    const juce::String& id() const noexcept { return pluginId; }

protected:
    virtual void prepareDsp(double sampleRate, int maxBlock, int channels) = 0;
    virtual void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;
    virtual void resetDsp() = 0;
    virtual int dspLatencySamples() const { return 0; }
    virtual int maxDspLatencySamples() const { return dspLatencySamples(); }
    virtual juce::ValueTree extraState() { return {}; }
    virtual void restoreExtraState(const juce::ValueTree&) {}

    // Parameter access. Resolve once (constructor or prepareDsp) and keep the reference: both lookups scan the table.
    struct ParamRef
    {
        std::atomic<float>* raw = nullptr;
        float get() const noexcept { return raw != nullptr ? raw->load(std::memory_order_relaxed) : 0.0f; }
        bool on() const noexcept { return get() >= 0.5f; }
    };
    ParamRef paramRef(ParamId id) const noexcept;
    float param(ParamId id) const noexcept; // convenience: table scan + atomic load
    Smoother<float>& smoothed(ParamId id) noexcept; // table scan; cache the reference
    void updateSmoothers() noexcept; // called by the base at the start of each block
    void snapSmoothersToTargets() noexcept; // jump every smoother to its parameter value (preset load, tests)

    // Subclass contract:
    //  - processDsp() sees any block size <= the prepareToPlay maximum (the base splits larger host blocks);
    //    main input channels are 0..getMainBusNumInputChannels()-1, sidechain channels follow (use getBusBuffer).
    //    For mono-in / stereo-out the base copies the processed channel 0 to the other outputs afterwards.
    //  - resetDsp() may be called from the audio thread (NaN guard) and must not allocate.
    //  - dspLatencySamples() may change at runtime; maxDspLatencySamples() must bound every value it can take.
private:
    void processInternal(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hostBypass);
    void processSubBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hostBypass);
    void resetMeters() noexcept;

    const ParamTable& paramTable;
    juce::String pluginId;
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<AbcdSlots> abcd;
    std::unique_ptr<PresetManager> presetManager;
    juce::ValueTree ui{"UI"};

    struct SmoothedParam
    {
        std::atomic<float>* raw = nullptr;
        Smoother<float> smoother;
        float rampMs = 20.0f;
    };
    std::vector<SmoothedParam> smoothers;      // one per table row (unused rows keep raw == nullptr)
    std::vector<int> smoothedIndexByParam;      // index into smoothers by table index
    std::atomic<float>* bypassRaw = nullptr;
    int trimIndex = -1;

    std::atomic<bool> delta{false};
    std::atomic<int> nanCount{0};

    juce::AudioBuffer<float> dryBuffer;
    IntegerDelay<float> dryDelay;
    Smoother<float> bypassMix;
    Smoother<float>* trimSmoother = nullptr;
    DbGainCache trimGain;
    float matchGain = 1.0f;
    PeakMeter peakIn[2], peakOut[2];
    RmsMeter rmsIn[2], rmsOut[2];
    RmsMeter matchIn, matchOut;
    MeterFifo<MeterFrame, 64> meters;
    double currentSampleRate = 48000.0;
    int preparedChannels = 0;
    int reportedLatency = 0;
    bool prepared = false;
};
} // namespace mm
