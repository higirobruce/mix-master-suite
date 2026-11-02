#include "tools/render/PluginTarget.h"
#include <cmath>

namespace mmr
{
std::unique_ptr<PluginTarget> PluginTarget::load(const juce::String& path, juce::String& error)
{
    auto t = std::unique_ptr<PluginTarget>(new PluginTarget());
    juce::addDefaultFormatsToManager(t->formats);
    juce::OwnedArray<juce::PluginDescription> found;
    juce::KnownPluginList list;
    for (auto* f : t->formats.getFormats())
        if (f->fileMightContainThisPluginType(path))
            list.scanAndAddFile(path, true, found, *f);
    if (found.isEmpty())
    {
        error = "no plugin found at " + path;
        return nullptr;
    }
    t->description = *found[0];
    if (!t->createInstance(error))
        return nullptr;
    return t;
}

bool PluginTarget::createInstance(juce::String& error)
{
    instance = formats.createPluginInstance(description, preparedRate, preparedBlock, error);
    return instance != nullptr;
}

bool PluginTarget::prepare(double sampleRate, int maxBlock, const Job& job, juce::String& error)
{
    preparedRate = sampleRate;
    preparedBlock = maxBlock;
    preparedJob = job;
    auto layout = instance->getBusesLayout();
    auto set = [](int n) { return n == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo(); };
    if (layout.inputBuses.size() > 0)
        layout.inputBuses.set(0, set(job.inChannels));
    if (layout.outputBuses.size() > 0)
        layout.outputBuses.set(0, set(job.outChannels));
    if (layout.inputBuses.size() > 1)
        layout.inputBuses.set(1, job.sidechainChannels > 0 ? set(job.sidechainChannels) : juce::AudioChannelSet::disabled());
    if (!instance->setBusesLayout(layout))
    {
        error = "plugin rejected bus layout in=" + juce::String(job.inChannels) + " out=" + juce::String(job.outChannels);
        return false;
    }
    instance->setNonRealtime(true);
    playHead.set(job.bpm);
    instance->setPlayHead(&playHead);
    instance->prepareToPlay(sampleRate, maxBlock);
    return true;
}

void PluginTarget::release() { instance->releaseResources(); }

juce::AudioProcessorParameter* PluginTarget::findParam(const juce::String& id) const
{
    const juce::String hashed(juce::VST3ClientExtensions::convertJuceParameterId(id, true));
    const juce::String hashedLegacy(juce::VST3ClientExtensions::convertJuceParameterId(id, false));
    for (auto* p : instance->getParameters())
    {
        if (auto* hp = dynamic_cast<juce::HostedAudioProcessorParameter*>(p))
        {
            const auto pid = hp->getParameterID();
            if (pid == id || pid == hashed || pid == hashedLegacy)
                return p;
        }
        else if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
        {
            if (wp->getParameterID() == id)
                return p;
        }
    }
    return nullptr;
}

bool PluginTarget::setParam(const juce::String& id, double real, const juce::var& ranges, juce::String& error)
{
    auto* p = findParam(id);
    if (p == nullptr)
    {
        error = "unknown parameter " + id;
        return false;
    }
    float norm = 0.0f;
    if (ranges.isObject() && ranges.hasProperty(id))
    {
        const auto r = ranges.getProperty(id, juce::var());
        const juce::String type = r.getProperty("type", "float").toString();
        const float lo = static_cast<float>(static_cast<double>(r.getProperty("min", 0.0)));
        const float hi = static_cast<float>(static_cast<double>(r.getProperty("max", 1.0)));
        const float skew = static_cast<float>(static_cast<double>(r.getProperty("skew", 1.0)));
        if (type == "bool")
            norm = real >= 0.5 ? 1.0f : 0.0f;
        else if (type == "choice")
        {
            const int n = static_cast<int>(r.getProperty("num_choices", 2));
            norm = n > 1 ? static_cast<float>(real) / static_cast<float>(n - 1) : 0.0f;
        }
        else
        {
            juce::NormalisableRange<float> range(lo, hi, 0.0f, skew);
            norm = range.convertTo0to1(juce::jlimit(lo, hi, static_cast<float>(real)));
        }
    }
    else if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
        norm = rp->convertTo0to1(static_cast<float>(real));
    else
        norm = p->getValueForText(juce::String(real, 6));
    p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
    return true;
}

void PluginTarget::process(juce::AudioBuffer<float>& buffer, bool hostBypass)
{
    midi.clear();
    if (hostBypass)
        instance->processBlockBypassed(buffer, midi);
    else
        instance->processBlock(buffer, midi);
    playHead.advance(buffer.getNumSamples());
}

juce::var PluginTarget::paramsDump() const
{
    juce::Array<juce::var> arr;
    for (auto* p : instance->getParameters())
    {
        auto* o = new juce::DynamicObject();
        if (auto* hp = dynamic_cast<juce::HostedAudioProcessorParameter*>(p))
            o->setProperty("id", hp->getParameterID());
        else if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
            o->setProperty("id", wp->getParameterID());
        o->setProperty("name", p->getName(128));
        o->setProperty("label", p->getLabel());
        o->setProperty("default_norm", p->getDefaultValue());
        o->setProperty("value_norm", p->getValue());
        o->setProperty("text", p->getCurrentValueAsText());
        o->setProperty("min_text", p->getText(0.0f, 64));
        o->setProperty("max_text", p->getText(1.0f, 64));
        arr.add(juce::var(o));
    }
    return juce::var(arr);
}

bool PluginTarget::stateRoundtrip(juce::String& error)
{
    juce::MemoryBlock state;
    instance->getStateInformation(state);
    instance->releaseResources();
    instance.reset();
    if (!createInstance(error))
        return false;
    instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    return prepare(preparedRate, preparedBlock, preparedJob, error);
}
} // namespace mmr
