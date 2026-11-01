#include "helpers/TestProcessor.h"
#include "helpers/Signals.h"
#include "plugin_base/StateIO.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using mm::test::TestProcessor;

namespace
{
juce::AudioBuffer<float> toBuffer(const std::vector<float>& x, int channels)
{
    juce::AudioBuffer<float> b(channels, static_cast<int>(x.size()));
    for (int ch = 0; ch < channels; ++ch)
        b.copyFrom(ch, 0, x.data(), static_cast<int>(x.size()));
    return b;
}

void setParam(TestProcessor& p, const char* id, float realValue)
{
    auto* param = p.apvts().getParameter(id);
    REQUIRE(param != nullptr);
    param->setValueNotifyingHost(param->convertTo0to1(realValue));
}

float getParam(TestProcessor& p, const char* id)
{
    return p.apvts().getRawParameterValue(id)->load();
}

double rmsDb(const juce::AudioBuffer<float>& b, int ch, int start, int end)
{
    double acc = 0.0;
    for (int n = start; n < end; ++n)
        acc += static_cast<double>(b.getSample(ch, n)) * b.getSample(ch, n);
    return 10.0 * std::log10(acc / (end - start) + 1e-30);
}

// Process a whole buffer through the processor in blocks of `block` (0 = one block).
void run(TestProcessor& p, juce::AudioBuffer<float>& buf, int block, bool hostBypass = false)
{
    juce::MidiBuffer midi;
    const int total = buf.getNumSamples();
    if (block <= 0)
        block = total;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min(block, total - pos);
        juce::AudioBuffer<float> view(buf.getArrayOfWritePointers(), buf.getNumChannels(), pos, n);
        if (hostBypass)
            p.processBlockBypassed(view, midi);
        else
            p.processBlock(view, midi);
    }
}
} // namespace

TEST_CASE("parameter layout from table")
{
    TestProcessor p;
    auto& apvts = p.apvts();
    for (auto* id : {"gain_db", "bypass", "trim_db"})
        REQUIRE(apvts.getParameter(id) != nullptr);
    auto* gain = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("gain_db"));
    REQUIRE(gain != nullptr);
    REQUIRE(gain->getNormalisableRange().start == -24.0f);
    REQUIRE(gain->getNormalisableRange().end == 24.0f);
    REQUIRE(gain->convertFrom0to1(gain->getDefaultValue()) == Catch::Approx(6.0206f));
    auto* trim = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("trim_db"));
    REQUIRE(trim->getNormalisableRange().end == 12.0f);
    REQUIRE(dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass")) != nullptr);
    REQUIRE(gain->getLabel() == "dB");
}

TEST_CASE("state round trip")
{
    TestProcessor a;
    setParam(a, "gain_db", -3.5f);
    setParam(a, "trim_db", 2.0f);
    a.slots().store(1);
    setParam(a, "gain_db", 4.0f);
    a.slots().store(1);
    setParam(a, "gain_db", -3.5f);
    a.slots().recall(0);
    setParam(a, "gain_db", -3.5f);
    juce::MemoryBlock mb;
    a.getStateInformation(mb);

    TestProcessor b;
    b.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    REQUIRE(getParam(b, "gain_db") == Catch::Approx(-3.5f));
    REQUIRE(getParam(b, "trim_db") == Catch::Approx(2.0f));
    REQUIRE(b.slots().active() == 0);
    b.slots().recall(1);
    REQUIRE(getParam(b, "gain_db") == Catch::Approx(4.0f));
}

TEST_CASE("state forward compat")
{
    TestProcessor p;
    setParam(p, "trim_db", 5.0f);
    juce::ValueTree state("MM_STATE");
    state.setProperty("version", 1, nullptr);
    state.setProperty("pluginId", "test_plugin", nullptr);
    juce::ValueTree params("PARAMETERS");
    juce::ValueTree g("PARAM");
    g.setProperty("id", "gain_db", nullptr);
    g.setProperty("value", -10.0f, nullptr);
    params.addChild(g, -1, nullptr);
    juce::ValueTree unknown("PARAM");
    unknown.setProperty("id", "unknown_param", nullptr);
    unknown.setProperty("value", 1.0f, nullptr);
    params.addChild(unknown, -1, nullptr);
    state.addChild(params, -1, nullptr);
    juce::MemoryBlock mb;
    {
        juce::MemoryOutputStream os(mb, false);
        state.writeToStream(os);
    }
    p.setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    REQUIRE(getParam(p, "gain_db") == Catch::Approx(-10.0f));
    REQUIRE(getParam(p, "trim_db") == Catch::Approx(0.0f)); // missing -> default

    // wrong plugin id -> rejected, state unchanged
    setParam(p, "gain_db", 3.0f);
    state.setProperty("pluginId", "other_plugin", nullptr);
    juce::MemoryBlock mb2;
    {
        juce::MemoryOutputStream os(mb2, false);
        state.writeToStream(os);
    }
    p.setStateInformation(mb2.getData(), static_cast<int>(mb2.getSize()));
    REQUIRE(getParam(p, "gain_db") == Catch::Approx(3.0f));
}

TEST_CASE("latency reported")
{
    TestProcessor p;
    p.prepareToPlay(48000.0, 64);
    REQUIRE(p.getLatencySamples() == TestProcessor::kLatency);
}

TEST_CASE("variable block sizes")
{
    const double sr = 48000.0;
    auto x = mm::test::sine(440.0, sr, 1.0, 0.5);
    auto ref = toBuffer(x, 2);
    auto var = toBuffer(x, 2);
    TestProcessor a, b;
    a.prepareToPlay(sr, 4096);
    b.prepareToPlay(sr, 4096);
    run(a, ref, 4096);
    juce::MidiBuffer midi;
    const int sizes[] = {0, 1, 16, 63, 64, 500, 4096};
    int pos = 0, k = 0;
    while (pos < var.getNumSamples())
    {
        const int n = std::min(sizes[k++ % 7], var.getNumSamples() - pos);
        juce::AudioBuffer<float> view(var.getArrayOfWritePointers(), 2, pos, n);
        b.processBlock(view, midi);
        pos += n;
    }
    float maxDiff = 0.0f;
    for (int n = 0; n < ref.getNumSamples(); ++n)
        maxDiff = std::max(maxDiff, std::abs(ref.getSample(0, n) - var.getSample(0, n)));
    REQUIRE(maxDiff < 1e-6f);
}

TEST_CASE("bypass is gain matched")
{
    const double sr = 48000.0;
    for (bool host : {false, true})
    {
        TestProcessor p;
        setParam(p, "gain_db", 12.0f);
        p.prepareToPlay(sr, 64);
        auto wet = toBuffer(mm::test::sine(1000.0, sr, 1.0, 0.1), 2);
        run(p, wet, 64);
        const double wetDb = rmsDb(wet, 0, 24000, 48000);
        if (!host)
            setParam(p, "bypass", 1.0f);
        auto byp = toBuffer(mm::test::sine(1000.0, sr, 0.5, 0.1), 2);
        run(p, byp, 64, host);
        const double bypDb = rmsDb(byp, 0, 19200, 24000); // 400-500 ms, after the 20 ms ramp
        REQUIRE(bypDb == Catch::Approx(wetDb).margin(0.1));
    }
}

TEST_CASE("delta listen")
{
    const double sr = 48000.0;
    TestProcessor p;
    p.prepareToPlay(sr, 64);
    p.setDeltaListen(true);
    auto x = mm::test::sine(1000.0, sr, 0.2, 0.25);
    auto buf = toBuffer(x, 2);
    run(p, buf, 64);
    for (int n = 4800; n < buf.getNumSamples(); ++n)
        REQUIRE(buf.getSample(0, n) == Catch::Approx(x[static_cast<size_t>(n - TestProcessor::kLatency)]).margin(1e-5));
}

TEST_CASE("bus layouts")
{
    TestProcessor p;
    auto layout = [&](juce::AudioChannelSet in, juce::AudioChannelSet out) {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add(in);
        l.outputBuses.add(out);
        return p.checkBusesLayoutSupported(l);
    };
    using CS = juce::AudioChannelSet;
    REQUIRE(layout(CS::mono(), CS::mono()));
    REQUIRE(layout(CS::stereo(), CS::stereo()));
    REQUIRE(layout(CS::mono(), CS::stereo()));
    REQUIRE_FALSE(layout(CS::create5point1(), CS::create5point1()));
    REQUIRE_FALSE(layout(CS::stereo(), CS::mono()));

    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add(CS::mono());
    l.outputBuses.add(CS::stereo());
    REQUIRE(p.setBusesLayout(l));
    p.prepareToPlay(48000.0, 64);
    auto x = mm::test::sine(1000.0, 48000.0, 0.1, 0.25);
    juce::AudioBuffer<float> buf(2, static_cast<int>(x.size()));
    buf.clear();
    buf.copyFrom(0, 0, x.data(), static_cast<int>(x.size()));
    run(p, buf, 64);
    for (int n = 0; n < buf.getNumSamples(); ++n)
        REQUIRE(buf.getSample(0, n) == buf.getSample(1, n));
    REQUIRE(std::abs(buf.getSample(0, 1000)) > 0.0f);
}

TEST_CASE("nan guard")
{
    TestProcessor p;
    p.prepareToPlay(48000.0, 64);
    p.injectNaN = true;
    auto buf = toBuffer(mm::test::sine(1000.0, 48000.0, 0.01, 0.5), 2);
    run(p, buf, 64);
    for (int n = 0; n < buf.getNumSamples(); ++n)
        REQUIRE(std::isfinite(buf.getSample(0, n)));
    REQUIRE(buf.getSample(0, 5) == 0.0f);
    REQUIRE(p.nanCounter().load() >= 1);
}

TEST_CASE("preset file round trip")
{
    TestProcessor a;
    setParam(a, "gain_db", -7.25f);
    auto preset = mm::StateIO::makePreset(a.apvts(), a.table(), "My preset", juce::StringArray{"vocal", "bright"});
    const juce::String xml = preset.toXmlString();
    auto parsed = juce::ValueTree::fromXml(xml);
    REQUIRE(parsed.isValid());
    REQUIRE(parsed.getType().toString() == "PRESET");
    REQUIRE(parsed["name"].toString() == "My preset");
    const auto meta = juce::JSON::parse(parsed["meta"].toString());
    REQUIRE(meta.getProperty("tags", {}).getArray()->size() == 2);

    TestProcessor b;
    REQUIRE(mm::StateIO::applyPreset(parsed, b.apvts(), b.table(), nullptr));
    REQUIRE(getParam(b, "gain_db") == Catch::Approx(-7.25f));
}

TEST_CASE("smoothed parameter ramps")
{
    const double sr = 48000.0;
    TestProcessor p;
    setParam(p, "gain_db", 0.0f);
    p.prepareToPlay(sr, 64);
    auto buf = toBuffer(std::vector<float>(static_cast<size_t>(sr * 0.1), 1.0f), 2);
    juce::MidiBuffer midi;
    const float maxRatio = mm::dbToGain(24.0f / static_cast<float>(0.02 * sr)) * (1.0f + 1e-3f);
    for (int pos = 0; pos < buf.getNumSamples(); pos += 64)
    {
        if (pos == 640)
            setParam(p, "gain_db", 24.0f);
        juce::AudioBuffer<float> view(buf.getArrayOfWritePointers(), 2, pos, 64);
        p.processBlock(view, midi);
    }
    for (int n = TestProcessor::kLatency + 1; n < buf.getNumSamples(); ++n)
    {
        const float prev = buf.getSample(0, n - 1), cur = buf.getSample(0, n);
        if (prev > 0.0f)
            REQUIRE(cur / prev <= maxRatio);
    }
    REQUIRE(buf.getSample(0, buf.getNumSamples() - 1) == Catch::Approx(mm::dbToGain(24.0f)).margin(1e-3));
}
