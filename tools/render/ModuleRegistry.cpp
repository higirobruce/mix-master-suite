#include "tools/render/ModuleRegistry.h"
#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/MatchedBiquad.h"
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
// Biquad. params: type {0 peak,1 lowshelf,2 highshelf,3 lowpass,4 highpass,5 notch,6 allpass}, freq_hz, gain_db, q,
// precision {0 float, 1 double}, design {0 rbj, 1 matched (notch/allpass fall back to rbj)}
class BiquadModule : public Module
{
public:
    void prepare(double sampleRate, int, int channels) override
    {
        sr = sampleRate;
        numCh = channels > 2 ? 2 : channels;
        update();
        reset();
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "type") type = static_cast<int>(v);
        else if (id == "freq_hz") freq = v;
        else if (id == "gain_db") gain = v;
        else if (id == "q") q = v;
        else if (id == "precision") useDouble = v >= 0.5;
        else if (id == "design") matched = v >= 0.5;
        update();
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                buf[ch][i] = useDouble ? static_cast<float>(bd[ch].process(static_cast<double>(buf[ch][i])))
                                       : bf[ch].process(buf[ch][i]);
    }
    void reset() override
    {
        for (auto& b : bf) b.reset();
        for (auto& b : bd) b.reset();
    }

private:
    void update()
    {
        mm::BiquadCoeffs<double> c;
        if (matched && type <= 4)
        {
            switch (type)
            {
            case 0: c = mm::matched::peak(sr, freq, gain, q); break;
            case 1: c = mm::matched::lowShelf(sr, freq, gain, q); break;
            case 2: c = mm::matched::highShelf(sr, freq, gain, q); break;
            case 3: c = mm::matched::lowPass(sr, freq, q); break;
            default: c = mm::matched::highPass(sr, freq, q); break;
            }
            for (auto& b : bd) b.setCoeffs(c);
            for (auto& b : bf) b.setCoeffs(mm::BiquadCoeffs<float>::fromDouble(c));
            return;
        }
        switch (type)
        {
        case 0: c = mm::rbj::peak(sr, freq, gain, q); break;
        case 1: c = mm::rbj::lowShelf(sr, freq, gain, q); break;
        case 2: c = mm::rbj::highShelf(sr, freq, gain, q); break;
        case 3: c = mm::rbj::lowPass(sr, freq, q); break;
        case 4: c = mm::rbj::highPass(sr, freq, q); break;
        case 5: c = mm::rbj::notch(sr, freq, q); break;
        default: c = mm::rbj::allPass(sr, freq, q); break;
        }
        for (auto& b : bd) b.setCoeffs(c);
        for (auto& b : bf) b.setCoeffs(mm::BiquadCoeffs<float>::fromDouble(c));
    }
    mm::Biquad<float> bf[2];
    mm::Biquad<double> bd[2];
    double sr = 48000.0, freq = 1000.0, gain = 0.0, q = 0.707;
    int type = 0, numCh = 2;
    bool useDouble = true;
    bool matched = false;
};
} // namespace

void registerCoreModules()
{
    auto& r = ModuleRegistry::instance();
    r.add("identity", [] { return std::make_unique<IdentityModule>(); });
    r.add("smoother", [] { return std::make_unique<SmootherModule>(); });
    r.add("biquad", [] { return std::make_unique<BiquadModule>(); });
}
} // namespace mmr
