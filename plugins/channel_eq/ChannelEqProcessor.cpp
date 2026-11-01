#include "plugins/channel_eq/ChannelEqProcessor.h"
#include "Params.h"
#include "channel_eq_data.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/MatchedBiquad.h"
#include "core/filters/Transfer.h"
#include "plugins/channel_eq/ChannelEqEditor.h"
#include <cmath>

namespace ep = mm::plugins::channel_eq;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &channel_eq_data::getNamedResource;
    f.names = channel_eq_data::namedResourceList;
    f.count = channel_eq_data::namedResourceListSize;
    return f;
}

// Butterworth section Qs per slope (ADR-0003); -1 marks the one-pole section
struct SlopeSpec
{
    bool onePole;
    int numBiquads;
    double q[4];
};
const SlopeSpec kSlopes[6] = {
    {true, 0, {}},                                            // 6
    {false, 1, {0.70710678}},                                 // 12
    {true, 1, {1.0}},                                         // 18
    {false, 2, {0.54119610, 1.30656296}},                     // 24
    {false, 3, {0.51763809, 0.70710678, 1.93185165}},         // 36 (6th order: three biquads)
    {false, 4, {0.50979558, 0.60134489, 0.89997622, 2.56291545}}, // 48
};
const char* kBandIds[5][5] = {
    {ep::id::b1_enabled, ep::id::b1_type, ep::id::b1_freq_hz, ep::id::b1_gain_db, ep::id::b1_q},
    {ep::id::b2_enabled, ep::id::b2_type, ep::id::b2_freq_hz, ep::id::b2_gain_db, ep::id::b2_q},
    {ep::id::b3_enabled, ep::id::b3_type, ep::id::b3_freq_hz, ep::id::b3_gain_db, ep::id::b3_q},
    {ep::id::b4_enabled, ep::id::b4_type, ep::id::b4_freq_hz, ep::id::b4_gain_db, ep::id::b4_q},
    {ep::id::b5_enabled, ep::id::b5_type, ep::id::b5_freq_hz, ep::id::b5_gain_db, ep::id::b5_q},
};
} // namespace

ChannelEqProcessor::ChannelEqProcessor()
    : mm::PluginProcessorBase(ep::table(), ep::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory())
{
    for (int i = 0; i < kBands; ++i)
    {
        bands[static_cast<size_t>(i)].enabled = paramRef(kBandIds[i][0]);
        bands[static_cast<size_t>(i)].type = paramRef(kBandIds[i][1]);
        bands[static_cast<size_t>(i)].freq = &smoothed(kBandIds[i][2]);
        bands[static_cast<size_t>(i)].gain = &smoothed(kBandIds[i][3]);
        bands[static_cast<size_t>(i)].q = &smoothed(kBandIds[i][4]);
    }
    hp.enabled = paramRef(ep::id::hp_enabled);
    hp.slope = paramRef(ep::id::hp_slope);
    hp.freq = &smoothed(ep::id::hp_freq_hz);
    lp.enabled = paramRef(ep::id::lp_enabled);
    lp.slope = paramRef(ep::id::lp_slope);
    lp.freq = &smoothed(ep::id::lp_freq_hz);
    qMode = paramRef(ep::id::q_mode);
    oversampleRef = paramRef(ep::id::oversample);
    analyserRef = paramRef(ep::id::analyser);
}

void ChannelEqProcessor::prepareDsp(double sampleRate, int maxBlock, int channels)
{
    hostRate = sampleRate;
    oversampler.prepare(sampleRate, maxBlock, juce::jmin(channels, kMaxChannels), 2);
    maxOsLatency = oversampler.latencySamples();
    analyserPre.prepare(sampleRate, 12, juce::jmin(channels, kMaxChannels));
    analyserPost.prepare(sampleRate, 12, juce::jmin(channels, kMaxChannels));
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        hpOnePole[static_cast<size_t>(ch)].prepare(sampleRate * 2.0); // re-set in updateDesign
        lpOnePole[static_cast<size_t>(ch)].prepare(sampleRate * 2.0);
    }
    oversampleOn = oversampleRef.on();
    designRate = oversampleOn ? sampleRate * 2.0 : sampleRate;
    resetDsp();
    updateDesign(true);
}

void ChannelEqProcessor::resetDsp()
{
    for (auto& chBands : bandFilters)
        for (auto& f : chBands)
            f.reset();
    for (auto& ch : hpFilters)
        for (auto& f : ch)
            f.reset();
    for (auto& ch : lpFilters)
        for (auto& f : ch)
            f.reset();
    for (auto& f : hpOnePole)
        f.reset();
    for (auto& f : lpOnePole)
        f.reset();
    oversampler.reset();
}

void ChannelEqProcessor::designFilter(FilterRefs& f, bool highPass, double rate)
{
    const auto& s = kSlopes[juce::jlimit(0, 5, f.lastSlope)];
    const double fc = juce::jlimit(1.0, 0.49 * rate, f.lastFreq);
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        if (highPass)
        {
            hpOnePole[static_cast<size_t>(ch)].prepare(rate);
            hpOnePole[static_cast<size_t>(ch)].setCutoff(fc);
        }
        else
        {
            lpOnePole[static_cast<size_t>(ch)].prepare(rate);
            lpOnePole[static_cast<size_t>(ch)].setCutoff(fc);
        }
    }
    f.prevCoeffs = f.coeffs;
    const int prevBiquads = f.numBiquads;
    f.useOnePole = s.onePole;
    f.numBiquads = s.numBiquads;
    for (int i = 0; i < f.numBiquads; ++i)
        f.coeffs[static_cast<size_t>(i)] = highPass ? mm::rbj::highPass(rate, fc, s.q[i]) : mm::rbj::lowPass(rate, fc, s.q[i]);
    f.ramping = f.wasActive && f.active && prevBiquads == f.numBiquads; // same structure: interpolate the cutoff change
    f.wasActive = f.active;
    if (!f.ramping)
    {
        auto& filters = highPass ? hpFilters : lpFilters;
        for (auto& ch : filters)
            for (int i = 0; i < f.numBiquads; ++i)
                ch[static_cast<size_t>(i)].setCoeffs(f.coeffs[static_cast<size_t>(i)]);
    }
}

void ChannelEqProcessor::designBand(BandRefs& b, double rate)
{
    b.prevCoeffs = b.coeffs;
    b.ramping = b.active;
    const double g = b.lastGain;
    double q = b.lastQ;
    if (b.lastType == 0 && lastAnalogQ)
        q *= 1.0 + std::abs(g) / 12.0; // proportional Q (ADR-0003)
    const double f = juce::jlimit(1.0, 0.49 * rate, b.lastFreq);
    switch (b.lastType)
    {
    case 1: b.coeffs = mm::matched::lowShelf(rate, f, g, q); break;
    case 2: b.coeffs = mm::matched::highShelf(rate, f, g, q); break;
    default: b.coeffs = mm::matched::peak(rate, f, g, q); break;
    }
    const bool wasActive = b.active;
    b.active = b.lastEnabled && std::abs(g) > 1e-9;
    if (!wasActive && b.active)
    {
        // band switching on: ramp from the same design at a vanishing gain (poles already in place; interpolating
        // from the identity would drag the poles across the plane and wobble the gain)
        const double g0 = g > 0.0 ? 1e-3 : -1e-3;
        switch (b.lastType)
        {
        case 1: b.prevCoeffs = mm::matched::lowShelf(rate, f, g0, q); break;
        case 2: b.prevCoeffs = mm::matched::highShelf(rate, f, g0, q); break;
        default: b.prevCoeffs = mm::matched::peak(rate, f, g0, q); break;
        }
        b.ramping = true;
    }
    else
        b.ramping = b.ramping && wasActive && b.active;
    if (!b.ramping)
        for (auto& ch : bandFilters)
            for (int i = 0; i < kBands; ++i)
                if (&bands[static_cast<size_t>(i)] == &b)
                    ch[static_cast<size_t>(i)].setCoeffs(b.coeffs);
}

namespace
{
inline mm::BiquadCoeffs<double> lerp(const mm::BiquadCoeffs<double>& a, const mm::BiquadCoeffs<double>& b, double t) noexcept
{
    return {a.b0 + (b.b0 - a.b0) * t, a.b1 + (b.b1 - a.b1) * t, a.b2 + (b.b2 - a.b2) * t, a.a1 + (b.a1 - a.a1) * t,
            a.a2 + (b.a2 - a.a2) * t};
}
} // namespace

void ChannelEqProcessor::updateDesign(bool force)
{
    const bool os = oversampleRef.on();
    const bool analog = qMode.on();
    if (os != oversampleOn || analog != lastAnalogQ)
    {
        oversampleOn = os;
        designRate = os ? hostRate * 2.0 : hostRate;
        lastAnalogQ = analog;
        resetDsp();
        force = true;
    }
    const double rate = designRate;
    auto changed = [](double a, double b, double tol) { return std::abs(a - b) > tol; };

    for (auto& b : bands)
    {
        const double freq = b.freq->current(), gain = b.gain->current(), q = b.q->current();
        const int type = static_cast<int>(std::lround(b.type.get()));
        const bool enabled = b.enabled.on();
        if (force || type != b.lastType || enabled != b.lastEnabled || changed(freq, b.lastFreq, 1e-3 * freq) ||
            changed(gain, b.lastGain, 1e-4) || changed(q, b.lastQ, 1e-4 * q))
        {
            b.lastFreq = freq;
            b.lastGain = gain;
            b.lastQ = q;
            b.lastType = type;
            b.lastEnabled = enabled;
            designBand(b, rate);
        }
    }
    for (int side = 0; side < 2; ++side)
    {
        FilterRefs& f = side == 0 ? hp : lp;
        const double freq = f.freq->current();
        const int slope = static_cast<int>(std::lround(f.slope.get()));
        const bool enabled = f.enabled.on();
        if (force || slope != f.lastSlope || enabled != f.lastEnabled || changed(freq, f.lastFreq, 1e-3 * freq))
        {
            f.lastFreq = freq;
            f.lastSlope = slope;
            f.lastEnabled = enabled;
            f.active = enabled;
            designFilter(f, side == 0, rate);
        }
    }
    if (force) // prepare / mode switch: apply the designs directly, no ramp (the state was reset)
    {
        for (int i = 0; i < kBands; ++i)
        {
            auto& b = bands[static_cast<size_t>(i)];
            b.ramping = false;
            for (auto& ch : bandFilters)
                ch[static_cast<size_t>(i)].setCoeffs(b.coeffs);
        }
        for (int side = 0; side < 2; ++side)
        {
            FilterRefs& f = side == 0 ? hp : lp;
            f.ramping = false;
            auto& filters = side == 0 ? hpFilters : lpFilters;
            for (auto& ch : filters)
                for (int i = 0; i < f.numBiquads; ++i)
                    ch[static_cast<size_t>(i)].setCoeffs(f.coeffs[static_cast<size_t>(i)]);
        }
    }
}

void ChannelEqProcessor::applyRamp(double t) noexcept
{
    for (int i = 0; i < kBands; ++i)
    {
        auto& b = bands[static_cast<size_t>(i)];
        if (b.ramping)
        {
            const auto c = lerp(b.prevCoeffs, b.coeffs, t);
            for (auto& ch : bandFilters)
                ch[static_cast<size_t>(i)].setCoeffs(c);
        }
    }
    for (int side = 0; side < 2; ++side)
    {
        FilterRefs& f = side == 0 ? hp : lp;
        if (!f.ramping)
            continue;
        auto& filters = side == 0 ? hpFilters : lpFilters;
        for (int i = 0; i < f.numBiquads; ++i)
        {
            const auto c = lerp(f.prevCoeffs[static_cast<size_t>(i)], f.coeffs[static_cast<size_t>(i)], t);
            for (auto& ch : filters)
                ch[static_cast<size_t>(i)].setCoeffs(c);
        }
    }
}

double ChannelEqProcessor::processSample(double v, int ch) noexcept
{
    const size_t c = static_cast<size_t>(ch);
    if (hp.active)
    {
        if (hp.useOnePole)
            v = hpOnePole[c].process(v);
        for (int i = 0; i < hp.numBiquads; ++i)
            v = hpFilters[c][static_cast<size_t>(i)].process(v);
    }
    for (int i = 0; i < kBands; ++i)
        if (bands[static_cast<size_t>(i)].active)
            v = bandFilters[c][static_cast<size_t>(i)].process(v);
    if (lp.active)
    {
        if (lp.useOnePole)
            v = lpOnePole[c].process(v);
        for (int i = 0; i < lp.numBiquads; ++i)
            v = lpFilters[c][static_cast<size_t>(i)].process(v);
    }
    return v;
}

void ChannelEqProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin(buffer.getNumChannels(), kMaxChannels);
    // advance the smoothers to the end of this block, then design from their current values
    for (auto& b : bands)
    {
        b.freq->skip(n);
        b.gain->skip(n);
        b.q->skip(n);
    }
    hp.freq->skip(n);
    lp.freq->skip(n);
    updateDesign(false);

    const int analyserMode = static_cast<int>(std::lround(analyserRef.get()));
    if (analyserMode == 1 || analyserMode == 3)
        analyserPre.push(buffer.getArrayOfReadPointers(), channels, n);

    bool anyRamp = hp.ramping || lp.ramping;
    for (const auto& b : bands)
        anyRamp = anyRamp || b.ramping;

    // channel-interleaved per-sample loop so interpolated coefficients apply to every channel at the same time
    if (oversampleOn)
    {
        juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(), static_cast<size_t>(channels), 0, static_cast<size_t>(n));
        auto up = oversampler.up(block);
        const size_t m = up.getNumSamples();
        for (size_t i = 0; i < m; ++i)
        {
            if (anyRamp)
                applyRamp(static_cast<double>(i + 1) / static_cast<double>(m));
            for (int ch = 0; ch < channels; ++ch)
            {
                float* p = up.getChannelPointer(static_cast<size_t>(ch));
                p[i] = static_cast<float>(processSample(static_cast<double>(p[i]), ch));
            }
        }
        oversampler.down(block);
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            if (anyRamp)
                applyRamp(static_cast<double>(i + 1) / static_cast<double>(n));
            for (int ch = 0; ch < channels; ++ch)
            {
                float* p = buffer.getWritePointer(ch);
                p[i] = static_cast<float>(processSample(static_cast<double>(p[i]), ch));
            }
        }
    }
    for (auto& b : bands)
        b.ramping = false;
    hp.ramping = lp.ramping = false;
    if (analyserMode == 2 || analyserMode == 3)
        analyserPost.push(buffer.getArrayOfReadPointers(), channels, n);
}

std::complex<double> ChannelEqProcessor::evaluateAt(double hz) const
{
    std::complex<double> h = 1.0;
    const double rate = designRate;
    if (hp.active)
    {
        if (hp.useOnePole)
            h *= hpOnePole[0].evaluate(hz);
        for (int i = 0; i < hp.numBiquads; ++i)
            h *= mm::evaluate(hp.coeffs[static_cast<size_t>(i)], rate, hz);
    }
    for (const auto& b : bands)
        if (b.active)
            h *= mm::evaluate(b.coeffs, rate, hz);
    if (lp.active)
    {
        if (lp.useOnePole)
            h *= lpOnePole[0].evaluate(hz);
        for (int i = 0; i < lp.numBiquads; ++i)
            h *= mm::evaluate(lp.coeffs[static_cast<size_t>(i)], rate, hz);
    }
    return h;
}

ChannelEqProcessor::BandView ChannelEqProcessor::band(int index) const noexcept
{
    const auto& b = bands[static_cast<size_t>(juce::jlimit(0, kBands - 1, index))];
    return {b.lastEnabled, b.lastType, b.lastFreq, b.lastGain, b.lastQ};
}

juce::AudioProcessorEditor* ChannelEqProcessor::createEditor() { return new ChannelEqEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChannelEqProcessor(); }
