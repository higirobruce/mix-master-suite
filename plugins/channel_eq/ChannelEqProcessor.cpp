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
                              factory(), ep::kPluginName)
{
    for (int i = 0; i < kBands; ++i)
    {
        bands[static_cast<size_t>(i)].enabled = paramRef(kBandIds[i][0]);
        bands[static_cast<size_t>(i)].type = paramRef(kBandIds[i][1]);
        bands[static_cast<size_t>(i)].freq = &smoothed(kBandIds[i][2]);
        bands[static_cast<size_t>(i)].gain = &smoothed(kBandIds[i][3]);
        bands[static_cast<size_t>(i)].q = &smoothed(kBandIds[i][4]);
        bands[static_cast<size_t>(i)].freqRaw = paramRef(kBandIds[i][2]);
        bands[static_cast<size_t>(i)].gainRaw = paramRef(kBandIds[i][3]);
        bands[static_cast<size_t>(i)].qRaw = paramRef(kBandIds[i][4]);
    }
    hp.enabled = paramRef(ep::id::hp_enabled);
    hp.slope = paramRef(ep::id::hp_slope);
    hp.freq = &smoothed(ep::id::hp_freq_hz);
    hp.freqRaw = paramRef(ep::id::hp_freq_hz);
    lp.enabled = paramRef(ep::id::lp_enabled);
    lp.slope = paramRef(ep::id::lp_slope);
    lp.freq = &smoothed(ep::id::lp_freq_hz);
    lp.freqRaw = paramRef(ep::id::lp_freq_hz);
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
    oversampleOn = oversampleRef.on();
    lastAnalogQ = qMode.on();
    designRate = oversampleOn ? sampleRate * 2.0 : sampleRate;
    switchStep = 1.0 / juce::jmax(1.0, kSwitchMs * 1e-3 * designRate);
    for (auto* f : {&hp, &lp})
    {
        f->onePoleRate = 0.0; // forces prepare() at the design rate
        f->pendingSlope = false;
    }
    resetDsp();
    updateDesign(true);
}

void ChannelEqProcessor::resetFilterState(bool highPass) noexcept
{
    if (highPass)
    {
        for (auto& ch : hpFilters)
            for (auto& f : ch)
                f.reset();
        for (auto& f : hpOnePole)
            f.reset();
    }
    else
    {
        for (auto& ch : lpFilters)
            for (auto& f : ch)
                f.reset();
        for (auto& f : lpOnePole)
            f.reset();
    }
}

void ChannelEqProcessor::resetDsp()
{
    for (auto& chBands : bandFilters)
        for (auto& f : chBands)
            f.reset();
    resetFilterState(true);
    resetFilterState(false);
    oversampler.reset();
}

namespace
{
// cutoff the switch ramp glides to when a filter is off: (near) identity at audio frequencies
inline double offCutoff(bool highPass, double rate) noexcept { return highPass ? 2.0 : 0.49 * rate; }
// eased: the first octave away from fc (where a resonant section still rings at the material it was tuned to) is
// taken slowly, the far octaves quickly
inline double glideCutoff(bool highPass, double rate, double fc, double onPos) noexcept
{
    const double off = offCutoff(highPass, rate);
    const double u = 1.0 - onPos; // 0 at the on position
    return std::exp(std::log(fc) + (std::log(off) - std::log(fc)) * u * u);
}
} // namespace

namespace
{
template <typename Sections>
void setSections(Sections& filters, int num, bool highPass, double fc, const double* q)
{
    for (auto& ch : filters)
        for (int i = 0; i < num; ++i)
            ch[static_cast<size_t>(i)].setParams(fc, q[i], 0.0);
    juce::ignoreUnused(highPass);
}
} // namespace

// Design one HP/LP from f.lastFreq / f.lastSlope at the switch position. direct: apply now (structure change,
// prepare); otherwise glide from the previous cutoff across the block (applyRamp).
void ChannelEqProcessor::designFilter(FilterRefs& f, bool highPass, double rate, bool direct)
{
    const auto& s = kSlopes[juce::jlimit(0, 5, f.lastSlope)];
    const double fcOn = juce::jlimit(1.0, 0.49 * rate, f.lastFreq);
    const double fc = juce::jlimit(1.0, 0.49 * rate, glideCutoff(highPass, rate, fcOn, f.onPos));
    auto& sections = highPass ? hpFilters : lpFilters;
    if (f.onePoleRate != rate) // prepare() resets the state: only when the rate changes
    {
        f.onePoleRate = rate;
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            if (highPass) hpOnePole[static_cast<size_t>(ch)].prepare(rate);
            else lpOnePole[static_cast<size_t>(ch)].prepare(rate);
            for (auto& sec : sections[static_cast<size_t>(ch)])
            {
                sec.prepare(rate);
                sec.setMode(highPass ? mm::SvfMode::HighPass : mm::SvfMode::LowPass);
            }
        }
        direct = true;
    }
    f.prevOnePoleFc = f.onePoleFc;
    f.onePoleFc = fc;
    f.prevCoeffs = f.coeffs;
    const int prevBiquads = f.numBiquads;
    const bool prevOnePole = f.useOnePole;
    f.useOnePole = s.onePole;
    f.numBiquads = s.numBiquads;
    for (int i = 0; i < f.numBiquads; ++i)
        f.coeffs[static_cast<size_t>(i)] = highPass ? mm::rbj::highPass(rate, fc, s.q[i]) : mm::rbj::lowPass(rate, fc, s.q[i]);
    const bool sameStructure = prevBiquads == f.numBiquads && prevOnePole == f.useOnePole;
    f.ramping = !direct && f.active && sameStructure; // same structure: glide the cutoff across the block
    if (!f.ramping)
    {
        setSections(sections, f.numBiquads, highPass, fc, s.q);
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            if (highPass) hpOnePole[static_cast<size_t>(ch)].setCutoff(fc);
            else lpOnePole[static_cast<size_t>(ch)].setCutoff(fc);
        }
    }
}

// Design one band at the switch position (effective gain = gain * onPos). direct: apply now (prepare / mode switch).
void ChannelEqProcessor::designBand(int index, double rate, bool direct)
{
    auto& b = bands[static_cast<size_t>(index)];
    b.prevCoeffs = b.coeffs;
    // effective gain; a vanishing (not zero) gain keeps the poles in the design so the per-sample coefficient
    // interpolation stays linear in amplitude (interpolating towards the identity biquad collapses the gain in ~2 ms)
    double g = b.lastGain * b.onPos;
    if (std::abs(g) < 1e-3)
        g = (b.lastGain >= 0.0 ? 1e-3 : -1e-3);
    double q = b.lastQ;
    if (b.lastType == 0 && lastAnalogQ)
        q *= 1.0 + std::abs(b.lastGain) / 12.0; // proportional Q (ADR-0003): from the parameter gain, so the shape holds while switching
    const double f = juce::jlimit(1.0, 0.49 * rate, b.lastFreq);
    switch (b.lastType)
    {
    case 1: b.coeffs = mm::matched::lowShelf(rate, f, g, q); break;
    case 2: b.coeffs = mm::matched::highShelf(rate, f, g, q); break;
    default: b.coeffs = mm::matched::peak(rate, f, g, q); break;
    }
    b.ramping = !direct && b.active;
    if (!b.ramping)
        for (auto& ch : bandFilters)
            ch[static_cast<size_t>(index)].setCoeffs(b.coeffs);
}

namespace
{
inline mm::BiquadCoeffs<double> lerp(const mm::BiquadCoeffs<double>& a, const mm::BiquadCoeffs<double>& b, double t) noexcept
{
    return {a.b0 + (b.b0 - a.b0) * t, a.b1 + (b.b1 - a.b1) * t, a.b2 + (b.b2 - a.b2) * t, a.a1 + (b.a1 - a.a1) * t,
            a.a2 + (b.a2 - a.a2) * t};
}
// move a switch position towards its target by n samples' worth; returns true if it moved
inline bool advanceSwitch(double& pos, double target, double step, int n) noexcept
{
    if (pos == target)
        return false;
    const double d = step * static_cast<double>(n);
    pos = pos < target ? juce::jmin(target, pos + d) : juce::jmax(target, pos - d);
    return true;
}
} // namespace

void ChannelEqProcessor::syncMode()
{
    const bool os = oversampleRef.on();
    const bool analog = qMode.on();
    if (os != oversampleOn || analog != lastAnalogQ)
    {
        oversampleOn = os;
        designRate = os ? hostRate * 2.0 : hostRate;
        lastAnalogQ = analog;
        switchStep = 1.0 / juce::jmax(1.0, kSwitchMs * 1e-3 * designRate);
        for (auto* f : {&hp, &lp})
            f->onePoleRate = 0.0; // one-poles must be re-prepared at the new design rate
        resetDsp();
        updateDesign(true); // mode switches are configuration changes: designs apply directly (ADR-0003)
    }
}

// Called once per block (after the smoothers advanced to the block end). The designs made here are reached by the
// per-sample coefficient interpolation across this block; switch ramps (enable, gain through 0 dB, slope) move
// onPos by one block's worth so that every switch is a 20 ms glide whatever the block size.
void ChannelEqProcessor::updateDesign(bool force)
{
    const double rate = designRate;
    const int n = lastBlockSamples;
    auto changed = [](double a, double b, double tol) { return std::abs(a - b) > tol; };

    for (int i = 0; i < kBands; ++i)
    {
        auto& b = bands[static_cast<size_t>(i)];
        const double freq = b.freq->current(), gain = b.gain->current(), q = b.q->current();
        const int type = static_cast<int>(std::lround(b.type.get()));
        const bool enabled = b.enabled.on();
        const bool wantOn = enabled && std::abs(gain) > 1e-9;
        if (b.active && b.onTarget <= 0.0 && b.onPos <= 0.0 && !wantOn) // ramped to 0 dB: leave the chain
        {
            b.active = false;
            b.ramping = false;
        }
        b.onTarget = wantOn ? 1.0 : 0.0;
        bool redesign = force;
        if (wantOn && !b.active)
        {
            // switching on: fresh state, start from the identity (0 dB) and glide up
            b.active = true;
            b.onPos = 0.0;
            for (auto& ch : bandFilters)
                ch[static_cast<size_t>(i)].reset();
            b.lastFreq = freq;
            b.lastGain = gain;
            b.lastQ = q;
            b.lastType = type;
            b.lastEnabled = enabled;
            designBand(i, rate, true);
        }
        if (force)
            b.onPos = b.onTarget;
        if (b.active && advanceSwitch(b.onPos, b.onTarget, switchStep, n))
            redesign = true;
        if (type != b.lastType || enabled != b.lastEnabled || changed(freq, b.lastFreq, 1e-3 * freq) ||
            changed(gain, b.lastGain, 1e-4) || changed(q, b.lastQ, 1e-4 * q))
        {
            b.lastFreq = freq;
            b.lastGain = gain;
            b.lastQ = q;
            b.lastType = type;
            b.lastEnabled = enabled;
            redesign = true;
        }
        if (redesign && b.active)
            designBand(i, rate, force);
    }
    for (int side = 0; side < 2; ++side)
    {
        FilterRefs& f = side == 0 ? hp : lp;
        const bool highPass = side == 0;
        const double freq = f.freq->current();
        const int slope = static_cast<int>(std::lround(f.slope.get()));
        const bool enabled = f.enabled.on();
        bool direct = force, redesign = force;
        // a glide to the off position finished: apply a pending slope (then glide back in) or leave the chain
        if (f.active && f.onTarget <= 0.0 && f.onPos <= 0.0)
        {
            if (f.pendingSlope)
            {
                f.pendingSlope = false;
                f.lastSlope = slope;
                resetFilterState(highPass);
                direct = redesign = true;
            }
            else if (!enabled)
            {
                f.active = false;
                f.ramping = false;
            }
        }
        if (enabled && !f.active)
        {
            f.active = true;
            f.onPos = 0.0;
            f.lastSlope = slope;
            resetFilterState(highPass);
            direct = redesign = true;
        }
        f.lastEnabled = enabled;
        if (slope != f.lastSlope && !direct)
        {
            if (f.active && f.onPos > 0.0)
                f.pendingSlope = true; // structure change while audible: glide out first, switch, glide back
            else
            {
                f.lastSlope = slope;
                resetFilterState(highPass);
                direct = redesign = true;
            }
        }
        const double newTarget = (enabled && !f.pendingSlope) ? 1.0 : 0.0;
        if (newTarget != f.onTarget || redesign)
        {
            // a high-Q section swept quickly past a tone it resonates at releases its stored energy as a bump
            // (+5.5 dB measured at 48 dB/oct); glide at 5 ms per octave, 20 ms at least
            const double fcOn = juce::jlimit(1.0, 0.49 * rate, static_cast<double>(f.freq->current()));
            const double octaves = std::abs(std::log2(fcOn / offCutoff(highPass, rate)));
            f.glideStep = 1.0 / juce::jmax(1.0, juce::jmax(kSwitchMs, kGlideMsPerOctave * octaves) * 1e-3 * rate);
        }
        f.onTarget = newTarget;
        if (force)
            f.onPos = f.onTarget;
        if (f.active && advanceSwitch(f.onPos, f.onTarget, f.glideStep, n))
            redesign = true;
        if (changed(freq, f.lastFreq, 1e-3 * freq))
        {
            f.lastFreq = freq;
            redesign = true;
        }
        if (redesign && f.active)
            designFilter(f, highPass, rate, direct);
    }
}

void ChannelEqProcessor::applyRamp(double t, size_t sampleIndex) noexcept
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
        if (!f.ramping || (sampleIndex & 7) != 0)
            continue; // cutoff glides are re-designed every 8 samples (0.17 ms) on a log-frequency path: a coefficient
                      // lerp between distant cutoffs passes through resonant, non-Butterworth shapes
        const double fc = std::exp(std::log(f.prevOnePoleFc) + (std::log(f.onePoleFc) - std::log(f.prevOnePoleFc)) * t);
        const auto& sl = kSlopes[juce::jlimit(0, 5, f.lastSlope)];
        setSections(side == 0 ? hpFilters : lpFilters, f.numBiquads, side == 0, fc, sl.q);
        if (f.useOnePole)
            for (int ch = 0; ch < kMaxChannels; ++ch)
            {
                if (side == 0) hpOnePole[static_cast<size_t>(ch)].setCutoff(fc);
                else lpOnePole[static_cast<size_t>(ch)].setCutoff(fc);
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
    const int channels = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), kMaxChannels);
    // advance the smoothers to the end of this block, then design from their current values
    for (auto& b : bands)
    {
        b.freq->skip(n);
        b.gain->skip(n);
        b.q->skip(n);
    }
    hp.freq->skip(n);
    lp.freq->skip(n);
    lastBlockSamples = oversampleOn ? 2 * n : n;
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
                applyRamp(static_cast<double>(i + 1) / static_cast<double>(m), i);
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
                applyRamp(static_cast<double>(i + 1) / static_cast<double>(n), static_cast<size_t>(i));
            for (int ch = 0; ch < channels; ++ch)
            {
                float* p = buffer.getWritePointer(ch);
                p[i] = static_cast<float>(processSample(static_cast<double>(p[i]), ch));
            }
        }
    }
    for (auto& b : bands)
        b.ramping = false;
    for (int side = 0; side < 2; ++side) // land exactly on the block's design
    {
        FilterRefs& f = side == 0 ? hp : lp;
        if (!f.ramping)
            continue;
        f.ramping = false;
        setSections(side == 0 ? hpFilters : lpFilters, f.numBiquads, side == 0, f.onePoleFc, kSlopes[juce::jlimit(0, 5, f.lastSlope)].q);
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            if (side == 0) hpOnePole[static_cast<size_t>(ch)].setCutoff(f.onePoleFc);
            else lpOnePole[static_cast<size_t>(ch)].setCutoff(f.onePoleFc);
        }
    }
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

std::complex<double> ChannelEqProcessor::staticResponseAt(double hz) const
{
    const double host = getSampleRate() > 0.0 ? getSampleRate() : hostRate;
    const double rate = oversampleRef.on() ? host * 2.0 : host;
    const bool analog = qMode.on();
    std::complex<double> h = 1.0;
    for (int side = 0; side < 2; ++side)
    {
        const FilterRefs& f = side == 0 ? hp : lp;
        if (!f.enabled.on())
            continue;
        const auto& s = kSlopes[juce::jlimit(0, 5, static_cast<int>(std::lround(f.slope.get())))];
        const double fc = juce::jlimit(1.0, 0.49 * rate, static_cast<double>(f.freqRaw.get()));
        if (s.onePole)
        {
            if (side == 0)
            {
                mm::OnePoleHP<double> op;
                op.prepare(rate);
                op.setCutoff(fc);
                h *= op.evaluate(hz);
            }
            else
            {
                mm::OnePoleLP<double> op;
                op.prepare(rate);
                op.setCutoff(fc);
                h *= op.evaluate(hz);
            }
        }
        for (int i = 0; i < s.numBiquads; ++i)
            h *= mm::evaluate(side == 0 ? mm::rbj::highPass(rate, fc, s.q[i]) : mm::rbj::lowPass(rate, fc, s.q[i]), rate, hz);
    }
    for (int i = 0; i < kBands; ++i)
    {
        const auto b = band(i);
        if (!b.enabled || std::abs(b.gainDb) < 1e-9)
            continue;
        double q = b.q;
        if (b.type == 0 && analog)
            q *= 1.0 + std::abs(b.gainDb) / 12.0;
        const double f = juce::jlimit(1.0, 0.49 * rate, b.freq);
        mm::BiquadCoeffs<double> c;
        switch (b.type)
        {
        case 1: c = mm::matched::lowShelf(rate, f, b.gainDb, q); break;
        case 2: c = mm::matched::highShelf(rate, f, b.gainDb, q); break;
        default: c = mm::matched::peak(rate, f, b.gainDb, q); break;
        }
        h *= mm::evaluate(c, rate, hz);
    }
    return h;
}

ChannelEqProcessor::BandView ChannelEqProcessor::band(int index) const noexcept
{
    const auto& b = bands[static_cast<size_t>(juce::jlimit(0, kBands - 1, index))];
    return {b.enabled.on(), static_cast<int>(std::lround(b.type.get())), static_cast<double>(b.freqRaw.get()),
            static_cast<double>(b.gainRaw.get()), static_cast<double>(b.qRaw.get())};
}

juce::AudioProcessorEditor* ChannelEqProcessor::createEditor() { return new ChannelEqEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChannelEqProcessor(); }
