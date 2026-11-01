#pragma once
#include "tools/render/Target.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace mmr
{
// A core DSP module exposed to the render loop. M0: identity, smoother. M1 registers the rest.
class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare(double sampleRate, int maxBlock, int channels) = 0;
    virtual void setParam(const std::string& id, double value) = 0;
    virtual void process(float* const* buffers, int channels, int numSamples) = 0;
    virtual int latencySamples() const { return 0; }
    virtual void reset() {}
};

class ModuleRegistry
{
public:
    using Factory = std::function<std::unique_ptr<Module>()>;
    static ModuleRegistry& instance();
    void add(const std::string& name, Factory f) { factories[name] = std::move(f); }
    std::unique_ptr<Module> create(const std::string& name) const;
    std::vector<std::string> names() const;

private:
    std::map<std::string, Factory> factories;
};

void registerCoreModules();

class ModuleTarget : public Target
{
public:
    explicit ModuleTarget(std::unique_ptr<Module> m) : module(std::move(m)) {}
    bool prepare(double sampleRate, int maxBlock, const Job& job, juce::String&) override;
    void release() override { module->reset(); }
    bool setParam(const juce::String& id, double realValue, const juce::var&, juce::String&) override;
    void process(juce::AudioBuffer<float>& buffer, bool hostBypass) override;
    int latencySamples() const override { return module->latencySamples(); }
    juce::var paramsDump() const override { return juce::var(juce::Array<juce::var>()); }
    bool stateRoundtrip(juce::String&) override { return true; }

private:
    std::unique_ptr<Module> module;
    int channels = 2;
};
} // namespace mmr
