#include "plugins/saturation/SaturationProcessor.h"
#include "Params.h"
#include "core/filters/BiquadDesign.h"
#include "plugins/saturation/SaturationEditor.h"
#include "saturation_data.h"
#include <cmath>
#include <numbers>

namespace sp = mm::plugins::saturation;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &saturation_data::getNamedResource;
    f.names = saturation_data::namedResourceList;
    f.count = saturation_data::namedResourceListSize;
    return f;
}
constexpr float kBiasScale = 0.3f; // operating-point shift at |bias| = 1 (shaper input units)

// small-signal slope of each odd shaper at the operating point b (drive compensation)
inline float slopeAt(int character, float b) noexcept
{
    switch (character)
    {
    case 0: { const float kb = std::numbers::pi_v<float> * 0.5f * b; return 1.0f / (1.0f + kb * kb); } // atan
    case 2: { const float t = std::tanh(b); return 1.0f - t * t; }                                        // tanh
    case 3: return std::abs(b) <= 0.5f ? 1.0f : 0.0f;                                                   // soft clip, linear region
    default: return 1.0f;
    }
}
} // namespace

float SaturationProcessor::staticStage(float x, int character, float g, float bias) noexcept
{
    const float b = kBiasScale * bias;
    switch (character)
    {
    case 1: // tube: its own operating point and normalisation
    {
        mm::TubeTransfer t;
        t.set(static_cast<double>(g), static_cast<double>(bias));
        return t.process(x);
    }
    case 0: return (mm::shape::atanS(x * g + b) - mm::shape::atanS(b)) / (g * slopeAt(0, b));
    case 2: return (std::tanh(x * g + b) - std::tanh(b)) / (g * slopeAt(2, b));
    default: return (mm::shape::softClip(x * g + b, 0.5f) - mm::shape::softClip(b, 0.5f)) / (g * std::max(slopeAt(3, b), 0.25f));
    }
}

std::array<float, SaturationProcessor::kHarmonics> SaturationProcessor::harmonicLevelsDb(float driveDb, int character, float bias) noexcept
{
    constexpr int n = 4800; // 1 kHz at a 192 kHz virtual rate: harmonics to 16 kHz stay far below Nyquist, 25 cycles
    constexpr double rate = 192000.0;
    const float g = mm::dbToGain(driveDb);
    std::array<float, kHarmonics> out{};
    std::array<float, n> y{};
    for (int i = 0; i < n; ++i)
        y[static_cast<size_t>(i)] = staticStage(0.25f * std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * static_cast<float>(i) / static_cast<float>(rate)), character, g, bias);
    for (int k = 1; k <= kHarmonics; ++k)
    {
        const double w = 2.0 * std::numbers::pi * 1000.0 * k / rate;
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            re += y[static_cast<size_t>(i)] * std::cos(w * i);
            im -= y[static_cast<size_t>(i)] * std::sin(w * i);
        }
        out[static_cast<size_t>(k - 1)] = mm::gainToDb(static_cast<float>(2.0 * std::hypot(re, im) / n), -200.0f);
    }
    return out;
}

float SaturationProcessor::referenceDriveDb(int character) noexcept
{
    auto thdPct = [character](float driveDb) {
        const auto h = harmonicLevelsDb(driveDb, character, 0.0f);
        double sum = 0.0;
        for (int k = 1; k < kHarmonics; ++k)
            sum += std::pow(10.0, h[static_cast<size_t>(k)] / 10.0);
        return 100.0 * std::sqrt(sum) / std::pow(10.0, h[0] / 20.0);
    };
    float lo = 0.0f, hi = 24.0f; // THD rises with drive for every character
    if (thdPct(lo) >= 1.0)
        return lo;
    if (thdPct(hi) <= 1.0)
        return hi;
    for (int i = 0; i < 24; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        (thdPct(mid) < 1.0 ? lo : hi) = mid;
    }
    return 0.5f * (lo + hi);
}

SaturationProcessor::SaturationProcessor()
    : mm::PluginProcessorBase(sp::table(), sp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), sp::kPluginName)
{
    character = paramRef(sp::id::character);
    (void) mm::TabulatedADAA1<mm::shape::tanhS, mm::shape::tanhAD1D>::antiderivative(0.0); // build the tables now
    (void) mm::TabulatedADAA1<mm::shape::atanS, mm::shape::atanAD1D>::antiderivative(0.0);
    drive = &smoothed(sp::id::drive_db);
    bias = &smoothed(sp::id::bias);
    hfRolloff = &smoothed(sp::id::hf_rolloff_hz);
    mix = &smoothed(sp::id::mix_pct);
    output = &smoothed(sp::id::output_db);
}

void SaturationProcessor::designFilters(double osRate, double host)
{
    // the emphasis pair is linear, so it runs at the host rate around the oversampled stage (commutes with the
    // resamplers); the RBJ shelf at -3 dB is the exact inverse of the +3 dB one
    for (auto& ch : dsp)
    {
        ch.preEmphasis.design(host, 10000.0, 3.0, true);
        ch.deEmphasis.design(host, 10000.0, 3.0, true);
        ch.deEmphasis.invert(); // exact inverse of the pre-emphasis
        ch.headBump.design(host, 60.0, 2.0, false);
        ch.rolloff.prepare(osRate);
        ch.dcBlock.prepare(host);
        ch.dcBlock.setCutoff(5.0);
    }
}

void SaturationProcessor::prepareDsp(double sampleRate, int maxBlock, int channels)
{
    hostRate = sampleRate;
    oversampler.prepare(sampleRate, maxBlock, juce::jmin(channels, kMaxChannels), 4);
    dryDelay.prepare(oversampler.latencySamples(), kMaxChannels);
    dryDelay.setDelay(oversampler.latencySamples());
    for (auto& d : dryBuf)
        d.assign(static_cast<size_t>(maxBlock), 0.0f);
    designFilters(oversampler.oversampledRate(), sampleRate);
    lastRolloff = -1.0f;
    resetDsp();
}

void SaturationProcessor::resetDsp()
{
    for (auto& ch : dsp)
        ch.reset();
    oversampler.reset();
    dryDelay.reset();
    lastCharacter = -1;
}

void SaturationProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), kMaxChannels);
    const float driveStartDb = drive->current(), mixStart = mix->current() * 0.01f, outStartDb = output->current();
    for (auto* s : {drive, bias, hfRolloff, mix, output})
        s->skip(n);
    const int chr = juce::jlimit(0, kCharacters - 1, static_cast<int>(std::lround(character.get())));
    if (chr != lastCharacter) // character switch: a configuration change, fresh stage state
    {
        for (auto& ch : dsp)
            ch.reset();
        lastCharacter = chr;
    }
    const float b = kBiasScale * bias->current();
    const float roll = hfRolloff->current();
    const bool rolloffOn = roll < 19999.0f;
    if (rolloffOn && std::abs(roll - lastRolloff) > 1e-3f * roll)
    {
        for (auto& ch : dsp)
            ch.rolloff.setCutoff(roll);
        lastRolloff = roll;
    }
    const bool tape = chr == 0;
    tube.set(static_cast<double>(mm::dbToGain(drive->current())), static_cast<double>(bias->current()));

    // dry copy delayed by the oversampler latency for the mix; tape pre-emphasis at the host rate
    for (int ch = 0; ch < channels; ++ch)
    {
        float* src = buffer.getWritePointer(ch);
        float* d = dryBuf[static_cast<size_t>(ch)].data();
        for (int i = 0; i < n; ++i)
            d[i] = dryDelay.process(src[i], ch);
        if (tape)
            for (int i = 0; i < n; ++i)
                src[i] = dsp[static_cast<size_t>(ch)].preEmphasis.process(src[i]);
    }

    juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(), static_cast<size_t>(channels), 0, static_cast<size_t>(n));
    auto up = oversampler.up(block);
    const int m = static_cast<int>(up.getNumSamples());
    const float gStart = mm::dbToGain(driveStartDb), gEnd = mm::dbToGain(drive->current());
    const float slope = juce::jmax(slopeAt(chr, b), 0.25f);
    const float compStart = 1.0f / (gStart * slope), compEnd = 1.0f / (gEnd * slope);
    const float offset = chr == 0 ? mm::shape::atanS(b) : (chr == 2 ? std::tanh(b) : mm::shape::softClip(b, 0.5f));
    const float invM = 1.0f / static_cast<float>(m);
    for (int ch = 0; ch < channels; ++ch)
    {
        auto& d = dsp[static_cast<size_t>(ch)];
        float* p = up.getChannelPointer(static_cast<size_t>(ch));
        auto stageLoop = [&](auto&& shaper) {
            for (int i = 0; i < m; ++i)
            {
                const float t = static_cast<float>(i + 1) * invM;
                const float g = gStart + (gEnd - gStart) * t;
                const float comp = compStart + (compEnd - compStart) * t;
                p[i] = (shaper(p[i] * g + b) - offset) * comp;
            }
        };
        switch (chr)
        {
        case 0: stageLoop([&](float x) { return d.atanStage.process(x); }); break; // tape: emphasis pair at the host rate
        case 1:
            for (int i = 0; i < m; ++i)
                p[i] = tube.process(p[i]);
            break;
        case 2: stageLoop([&](float x) { return d.tanhStage.process(x); }); break;
        default: stageLoop([&](float x) { return d.clipStage.process(x); }); break;
        }
        if (rolloffOn)
            for (int i = 0; i < m; ++i)
                p[i] = d.rolloff.process(p[i]);
    }
    oversampler.down(block);

    // output level and mix at the host rate
    const float oStart = mm::dbToGain(outStartDb), oEnd = mm::dbToGain(output->current());
    const float mixEnd = mix->current() * 0.01f;
    for (int ch = 0; ch < channels; ++ch)
    {
        auto& dch = dsp[static_cast<size_t>(ch)];
        float* w = buffer.getWritePointer(ch);
        const float* d = dryBuf[static_cast<size_t>(ch)].data();
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(n);
            const float o = oStart + (oEnd - oStart) * t;
            const float mx = mixStart + (mixEnd - mixStart) * t;
            float wet = w[i];
            if (tape)
                wet = dch.headBump.process(dch.deEmphasis.process(wet));
            wet = static_cast<float>(dch.dcBlock.process(static_cast<double>(wet)));
            w[i] = wet * o * mx + d[i] * (1.0f - mx);
        }
    }
}

juce::AudioProcessorEditor* SaturationProcessor::createEditor() { return new SaturationEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SaturationProcessor(); }
