#include "tools/render/ModuleRegistry.h"
#include "core/smoothing/Smoother.h"

namespace mmr
{
ModuleRegistry& ModuleRegistry::instance()
{
    static ModuleRegistry r;
    return r;
}

std::unique_ptr<Module> ModuleRegistry::create(const std::string& name) const
{
    auto it = factories.find(name);
    return it == factories.end() ? nullptr : it->second();
}

std::vector<std::string> ModuleRegistry::names() const
{
    std::vector<std::string> out;
    for (const auto& [k, v] : factories)
        out.push_back(k);
    return out;
}

bool ModuleTarget::prepare(double sampleRate, int maxBlock, const Job& job, juce::String&)
{
    channels = job.outChannels;
    module->prepare(sampleRate, maxBlock, channels);
    return true;
}

bool ModuleTarget::setParam(const juce::String& id, double realValue, const juce::var&, juce::String&)
{
    module->setParam(id.toStdString(), realValue);
    return true;
}

void ModuleTarget::process(juce::AudioBuffer<float>& buffer, bool hostBypass)
{
    if (hostBypass)
        return;
    module->process(buffer.getArrayOfWritePointers(), juce::jmin(channels, buffer.getNumChannels()),
                    buffer.getNumSamples());
}

namespace
{
class IdentityModule : public Module
{
public:
    void prepare(double, int, int) override {}
    void setParam(const std::string&, double) override {}
    void process(float* const*, int, int) override {}
};

// Outputs the smoother's value per sample (input ignored). params: target, ramp_ms, mode (0 linear, 1 exponential)
class SmootherModule : public Module
{
public:
    void prepare(double sampleRate, int, int) override
    {
        sr = sampleRate;
        s.prepare(sr, rampMs, mode);
        s.snap(0.0f);
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "target")
            s.setTarget(static_cast<float>(v));
        else if (id == "ramp_ms")
        {
            rampMs = v;
            s.prepare(sr, rampMs, mode);
        }
        else if (id == "mode")
        {
            mode = v >= 0.5 ? mm::SmoothMode::Exponential : mm::SmoothMode::Linear;
            s.prepare(sr, rampMs, mode);
        }
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float v = s.next();
            for (int ch = 0; ch < channels; ++ch)
                buf[ch][i] = v;
        }
    }

private:
    mm::Smoother<float> s;
    mm::SmoothMode mode = mm::SmoothMode::Linear;
    double sr = 48000.0, rampMs = 20.0;
};
} // namespace

void registerCoreModules()
{
    auto& r = ModuleRegistry::instance();
    r.add("identity", [] { return std::make_unique<IdentityModule>(); });
    r.add("smoother", [] { return std::make_unique<SmootherModule>(); });
}
} // namespace mmr
