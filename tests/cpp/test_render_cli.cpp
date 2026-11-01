// Smoke tests for the mm_render CLI. Paths come from compile definitions (MM_RENDER_PATH, MM_UTILITY_VST3_PATH,
// MM_TEST_TMP_PATH); environment variables of the same name without _PATH override them.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdlib>

namespace
{
juce::String setting(const char* env, const char* compiled)
{
    const char* e = std::getenv(env);
    return e != nullptr ? juce::String(e) : juce::String(compiled);
}

juce::File tmpDir()
{
    auto d = juce::File(setting("MM_TEST_TMP", MM_TEST_TMP_PATH));
    d.createDirectory();
    return d;
}

int runRender(const juce::File& job)
{
    const juce::String bin = setting("MM_RENDER", MM_RENDER_PATH);
    REQUIRE(juce::File(bin).existsAsFile());
    juce::ChildProcess p;
    REQUIRE(p.start(juce::StringArray{bin, job.getFullPathName()}));
    p.waitForProcessToFinish(60000);
    INFO(p.readAllProcessOutput());
    return static_cast<int>(p.getExitCode());
}

juce::AudioBuffer<float> readWav(const juce::File& f, double& sr)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(f.createInputStream().release(), true));
    REQUIRE(r != nullptr);
    sr = r->sampleRate;
    juce::AudioBuffer<float> b(static_cast<int>(r->numChannels), static_cast<int>(r->lengthInSamples));
    r->read(&b, 0, b.getNumSamples(), 0, true, true);
    return b;
}
} // namespace

TEST_CASE("mm_render identity module passes an impulse")
{
    auto dir = tmpDir();
    auto out = dir.getChildFile("identity.wav"), rep = dir.getChildFile("identity.json"), job = dir.getChildFile("identity.job.json");
    job.replaceWithText(R"({"module":"identity","sample_rate":48000,"block_size":64,"channels":2,
      "input":{"gen":"impulse","dur_s":0.1},"output":")" + out.getFullPathName().replace("\\", "\\\\") +
                        R"(","report":")" + rep.getFullPathName().replace("\\", "\\\\") + R"("})");
    REQUIRE(runRender(job) == 0);
    double sr = 0;
    auto b = readWav(out, sr);
    REQUIRE(sr == 48000.0);
    REQUIRE(b.getNumSamples() == 4800);
    REQUIRE(b.getSample(0, 0) == 1.0f);
    for (int n = 1; n < b.getNumSamples(); ++n)
        REQUIRE(b.getSample(0, n) == 0.0f);
    auto r = juce::JSON::parse(rep.loadFileAsString());
    REQUIRE(static_cast<int>(r.getProperty("nan_count", -1)) == 0);
    REQUIRE(static_cast<int>(r.getProperty("latency_samples", -1)) == 0);
}

TEST_CASE("mm_render hosts the Utility VST3 and applies gain")
{
    const juce::String vst3 = juce::File(setting("MM_UTILITY_VST3", MM_UTILITY_VST3_PATH)).getFullPathName();
    REQUIRE(juce::File(vst3).exists());
    auto dir = tmpDir();
    auto out = dir.getChildFile("util.wav"), rep = dir.getChildFile("util.json"), job = dir.getChildFile("util.job.json");
    job.replaceWithText(R"({"plugin":")" + vst3.replace("\\", "\\\\") + R"(","sample_rate":48000,"block_size":64,"channels":2,
      "params":{"gain_db":-6.0206},"input":{"gen":"sine","freq_hz":1000,"amp_db":0,"dur_s":0.5},
      "output":")" + out.getFullPathName().replace("\\", "\\\\") + R"(","report":")" + rep.getFullPathName().replace("\\", "\\\\") + R"("})");
    REQUIRE(runRender(job) == 0);
    auto r = juce::JSON::parse(rep.loadFileAsString());
    REQUIRE(static_cast<double>(r.getProperty("peak_db", 0.0)) == Catch::Approx(-6.02).margin(0.02));
    REQUIRE(static_cast<int>(r.getProperty("nan_count", -1)) == 0);
    REQUIRE(static_cast<int>(r.getProperty("latency_samples", -1)) == 0);
    auto params = r.getProperty("params", juce::var());
    REQUIRE(params.isArray());
    REQUIRE(params.getArray()->size() == 11);
}
