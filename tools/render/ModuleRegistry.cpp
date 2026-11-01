#include "tools/render/ModuleRegistry.h"
#include "core/common/Math.h"
#include "core/filters/AllPass.h"
#include "core/filters/Biquad.h"
#include "core/filters/FIRDesign.h"
#include "core/filters/FIRFilter.h"
#include "core/filters/LinkwitzRiley.h"
#include "core/delay/FractionalDelayLine.h"
#include "core/dynamics/Detector.h"
#include "core/oversampling/Oversampler.h"
#include "core/dynamics/EnvelopeFollower.h"
#include "core/dynamics/GainComputer.h"
#include "core/dynamics/Lookahead.h"
#include "core/dynamics/SidechainFilter.h"
#include "core/filters/OnePole.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/MatchedBiquad.h"
#include "core/filters/SVF.h"
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
// TPT SVF. params: mode {0 lowpass,1 highpass,2 bandpass,3 notch,4 peak,5 lowshelf,6 highshelf,7 allpass}, freq_hz, q, gain_db
class SvfModule : public Module
{
public:
    void prepare(double sampleRate, int, int) override
    {
        for (auto& f : svf)
        {
            f.prepare(sampleRate);
            f.setMode(mode);
            f.setParams(freq, q, gain);
        }
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "mode") mode = static_cast<mm::SvfMode>(static_cast<int>(v));
        else if (id == "freq_hz") freq = v;
        else if (id == "q") q = v;
        else if (id == "gain_db") gain = v;
        for (auto& f : svf)
        {
            f.setMode(mode);
            f.setParams(freq, q, gain);
        }
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                buf[ch][i] = svf[ch].process(buf[ch][i]);
    }
    void reset() override
    {
        for (auto& f : svf) f.reset();
    }

private:
    mm::SVF<float> svf[2];
    mm::SvfMode mode = mm::SvfMode::LowPass;
    double freq = 1000.0, q = 0.7071, gain = 0.0;
};
// One-pole. params: kind {0 highpass, 1 lowpass}, freq_hz
class OnePoleModule : public Module
{
public:
    void prepare(double sampleRate, int, int) override
    {
        for (auto& f : hp) { f.prepare(sampleRate); f.setCutoff(freq); }
        for (auto& f : lp) { f.prepare(sampleRate); f.setCutoff(freq); }
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "kind") lowpass = v >= 0.5;
        else if (id == "freq_hz") freq = v;
        for (auto& f : hp) f.setCutoff(freq);
        for (auto& f : lp) f.setCutoff(freq);
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                buf[ch][i] = lowpass ? lp[ch].process(buf[ch][i]) : hp[ch].process(buf[ch][i]);
    }
    void reset() override
    {
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
    }

private:
    mm::OnePoleHP<float> hp[2];
    mm::OnePoleLP<float> lp[2];
    double freq = 1000.0;
    bool lowpass = false;
};

// First-order all-pass. params: freq_hz
class AllPass1Module : public Module
{
public:
    void prepare(double sampleRate, int, int) override
    {
        for (auto& f : ap) { f.prepare(sampleRate); f.setCorner(freq); }
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "freq_hz") freq = v;
        for (auto& f : ap) f.setCorner(freq);
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                buf[ch][i] = ap[ch].process(buf[ch][i]);
    }
    void reset() override { for (auto& f : ap) f.reset(); }

private:
    mm::FirstOrderAllPass<float> ap[2];
    double freq = 1000.0;
};

// Second-order all-pass. params: freq_hz, q
class AllPass2Module : public Module
{
public:
    void prepare(double sampleRate, int, int) override
    {
        for (auto& f : ap) { f.prepare(sampleRate); f.setParams(freq, q); }
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "freq_hz") freq = v;
        else if (id == "q") q = v;
        for (auto& f : ap) f.setParams(freq, q);
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                buf[ch][i] = ap[ch].process(buf[ch][i]);
    }
    void reset() override { for (auto& f : ap) f.reset(); }

private:
    mm::SecondOrderAllPass<float> ap[2];
    double freq = 1000.0, q = 0.7071;
};
// Windowed-sinc FIR. params: taps, cutoff_norm, window {0 hann,1 hamming,2 blackman,3 kaiser}, beta
class FirModule : public Module
{
public:
    void prepare(double, int, int channels) override
    {
        fir.prepare(16385, channels);
        prepared = true;
        design();
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "taps") taps = static_cast<int>(v);
        else if (id == "cutoff_norm") cutoff = v;
        else if (id == "window") window = static_cast<mm::fir::Window>(static_cast<int>(v));
        else if (id == "beta") beta = v;
        if (prepared)
            design();
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels; ++ch)
            fir.process(buf[ch], n, ch);
    }
    int latencySamples() const override { return fir.latencySamples(); }
    void reset() override { fir.reset(); }

private:
    void design()
    {
        const auto h = mm::fir::windowedSinc(taps, cutoff, window, beta);
        fir.setTaps(h.data(), static_cast<int>(h.size()));
    }
    mm::FIRFilter<float> fir;
    bool prepared = false;
    int taps = 63;
    double cutoff = 0.25, beta = 8.6;
    mm::fir::Window window = mm::fir::Window::Kaiser;
};
// Linkwitz-Riley crossover (64-bit, as the spec budgets for crossovers). params: freq_hz, order {2,4,8},
// output {0 low, 1 high, 2 sum}
class LrCrossoverModule : public Module
{
public:
    void prepare(double sampleRate, int, int) override
    {
        for (auto& x : xo)
        {
            x.prepare(sampleRate);
            x.setOrder(order);
            x.setCrossover(freq);
        }
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "freq_hz") freq = v;
        else if (id == "order") order = v >= 8 ? Order::LR8 : (v >= 4 ? Order::LR4 : Order::LR2);
        else if (id == "output") output = static_cast<int>(v);
        for (auto& x : xo)
        {
            x.setOrder(order);
            x.setCrossover(freq);
        }
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                double lo, hi;
                xo[ch].process(static_cast<double>(buf[ch][i]), lo, hi);
                buf[ch][i] = static_cast<float>(output == 0 ? lo : (output == 1 ? hi : lo + hi));
            }
    }
    void reset() override { for (auto& x : xo) x.reset(); }

private:
    using Order = mm::LrOrder;
    mm::LinkwitzRiley<double> xo[2];
    double freq = 1000.0;
    Order order = Order::LR4;
    int output = 0;
};
// Detector + envelope follower; output = envelope in dB / 100 (so it fits a WAV). params: mode {0 peak, 1 rms},
// rms_ms, attack_ms, release_ms, auto_release
class EnvelopeModule : public Module
{
public:
    void prepare(double sampleRate, int, int) override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            det[ch].prepare(sampleRate);
            det[ch].setMode(mode);
            det[ch].setRmsWindowMs(rmsMs);
            env[ch].prepare(sampleRate);
            env[ch].setAttackMs(attackMs);
            env[ch].setReleaseMs(releaseMs);
            env[ch].setAutoRelease(autoRel);
        }
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "mode") mode = v >= 0.5 ? mm::DetectorMode::Rms : mm::DetectorMode::Peak;
        else if (id == "rms_ms") rmsMs = v;
        else if (id == "attack_ms") attackMs = v;
        else if (id == "release_ms") releaseMs = v;
        else if (id == "auto_release") autoRel = v >= 0.5;
        for (int ch = 0; ch < 2; ++ch)
        {
            det[ch].setMode(mode);
            det[ch].setRmsWindowMs(rmsMs);
            env[ch].setAttackMs(attackMs);
            env[ch].setReleaseMs(releaseMs);
            env[ch].setAutoRelease(autoRel);
        }
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float level = det[ch].process(buf[ch][i]);
                buf[ch][i] = env[ch].process(mm::gainToDb(level)) * 0.01f;
            }
    }
    void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            det[ch].reset();
            env[ch].reset();
        }
    }

private:
    mm::Detector<float> det[2];
    mm::EnvelopeFollower<float> env[2];
    mm::DetectorMode mode = mm::DetectorMode::Peak;
    double rmsMs = 50.0, attackMs = 10.0, releaseMs = 100.0;
    bool autoRel = false;
};
// Feed-forward compressor chain (Giannoulis et al. 2012, "ballistics on the gain"): sidechain filter -> detector
// -> dB -> static gain computer -> attack/release smoothing of the gain reduction (log domain) -> gain.
// params: threshold_db, ratio, knee_db, attack_ms, release_ms, lookahead_ms, sc_hp_hz, expander, range_db, detector {0 peak,1 rms}
class CompressorCoreModule : public Module
{
public:
    void prepare(double sampleRate, int, int channels) override
    {
        numCh = channels > 2 ? 2 : channels;
        for (int ch = 0; ch < 2; ++ch)
        {
            sc[ch].prepare(sampleRate);
            det[ch].prepare(sampleRate);
            env[ch].prepare(sampleRate);
        }
        la.prepare(sampleRate, 10.0, 2);
        for (auto& e : env)
            e.snapTo(0.0f); // the smoothed quantity is gain reduction: starts at 0 dB
        prepared = true;
        apply();
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "threshold_db") gp.thresholdDb = v;
        else if (id == "ratio") gp.ratio = v;
        else if (id == "knee_db") gp.kneeDb = v;
        else if (id == "expander") gp.expander = v >= 0.5;
        else if (id == "range_db") gp.rangeDb = v;
        else if (id == "attack_ms") attackMs = v;
        else if (id == "release_ms") releaseMs = v;
        else if (id == "lookahead_ms") lookaheadMs = v;
        else if (id == "sc_hp_hz") scHp = v;
        else if (id == "detector") rms = v >= 0.5;
        if (prepared)
            apply();
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            // stereo-linked: max of channel levels
            float level = 0.0f;
            for (int ch = 0; ch < channels && ch < 2; ++ch)
                level = std::max(level, det[ch].process(sc[ch].process(buf[ch][i])));
            if (lookaheadOn)
                level = la.maxEnvelope(level);
            // static reduction (positive dB) then ballistics: attack when reduction grows, release when it falls
            const float reduction = -gc.gainDb(mm::gainToDb(level));
            const float smoothed = env[0].process(reduction);
            const float gain = mm::dbToGain(-smoothed);
            for (int ch = 0; ch < channels && ch < 2; ++ch)
            {
                const float x = lookaheadOn ? la.delaySignal(buf[ch][i], ch) : buf[ch][i];
                buf[ch][i] = x * gain;
            }
        }
    }
    int latencySamples() const override { return lookaheadOn ? la.latencySamples() : 0; }
    void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            sc[ch].reset();
            det[ch].reset();
            env[ch].reset();
            env[ch].snapTo(0.0f);
        }
        la.reset();
    }

private:
    void apply()
    {
        gc.set(gp);
        for (int ch = 0; ch < 2; ++ch)
        {
            sc[ch].setHighPass(scHp);
            det[ch].setMode(rms ? mm::DetectorMode::Rms : mm::DetectorMode::Peak);
            env[ch].setAttackMs(attackMs);
            env[ch].setReleaseMs(releaseMs);
        }
        la.setMs(lookaheadMs);
        lookaheadOn = lookaheadMs > 0.0;
    }
    mm::SidechainFilter<float> sc[2];
    mm::Detector<float> det[2];
    mm::EnvelopeFollower<float> env[2];
    mm::GainComputer<float> gc;
    mm::Lookahead<float> la;
    mm::GainComputerParams gp{-20.0, 4.0, 0.0, false, -80.0};
    double attackMs = 1.0, releaseMs = 100.0, lookaheadMs = 0.0, scHp = 0.0;
    bool rms = false, lookaheadOn = false, prepared = false;
    int numCh = 2;
};
// Delay line with feedback (wet only: first echo at unity). params: delay_ms, interp {0 lagrange3, 1 thiran}, feedback, slew_ms
class DelayModule : public Module
{
public:
    void prepare(double sampleRate, int, int channels) override
    {
        sr = sampleRate;
        numCh = channels > 2 ? 2 : channels;
        line.prepare(sr, 5.0, 2);
        prepared = true;
        apply();
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "delay_ms") delayMs = v;
        else if (id == "interp") interp = v >= 0.5 ? mm::DelayInterp::Thiran1 : mm::DelayInterp::Lagrange3;
        else if (id == "feedback") feedback = static_cast<float>(v);
        else if (id == "slew_ms") slewMs = v;
        if (prepared)
            apply();
    }
    void process(float* const* buf, int channels, int n) override
    {
        for (int ch = 0; ch < channels && ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float out = line.read(ch);
                line.push(buf[ch][i] + feedback * out, ch);
                buf[ch][i] = out;
            }
    }
    void reset() override { line.reset(); }

private:
    void apply()
    {
        line.setInterp(interp);
        line.setSlewMs(slewMs);
        for (int ch = 0; ch < 2; ++ch)
            line.setDelaySamples(static_cast<float>(delayMs * 1e-3 * sr), ch);
    }
    mm::FractionalDelayLine<float> line;
    mm::DelayInterp interp = mm::DelayInterp::Lagrange3;
    double sr = 48000.0, delayMs = 100.0, slewMs = 20.0;
    float feedback = 0.0f;
    int numCh = 2;
    bool prepared = false;
};
// tanh at drive through the oversampler (aliasing test). params: factor {1,2,4,8}, drive_db
class OversampledTanhModule : public Module
{
public:
    void prepare(double sampleRate, int maxBlock, int channels) override
    {
        sr = sampleRate;
        block = maxBlock;
        ch = channels;
        os.prepare(sr, block, ch, factor);
    }
    void setParam(const std::string& id, double v) override
    {
        if (id == "factor")
        {
            factor = static_cast<int>(v);
            if (block > 0)
                os.prepare(sr, block, ch, factor);
        }
        else if (id == "drive_db")
            drive = mm::dbToGain(static_cast<float>(v));
    }
    void process(float* const* buf, int channels, int n) override
    {
        juce::dsp::AudioBlock<float> in(buf, static_cast<size_t>(channels), 0, static_cast<size_t>(n));
        auto up = os.up(in);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            float* p = up.getChannelPointer(c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                p[i] = std::tanh(p[i] * drive);
        }
        os.down(in);
    }
    int latencySamples() const override { return os.latencySamples(); }
    void reset() override { os.reset(); }

private:
    mm::Oversampler os;
    double sr = 48000.0;
    int block = 0, ch = 2, factor = 4;
    float drive = 1.0f;
};
} // namespace

void registerCoreModules()
{
    auto& r = ModuleRegistry::instance();
    r.add("identity", [] { return std::make_unique<IdentityModule>(); });
    r.add("smoother", [] { return std::make_unique<SmootherModule>(); });
    r.add("biquad", [] { return std::make_unique<BiquadModule>(); });
    r.add("svf", [] { return std::make_unique<SvfModule>(); });
    r.add("onepole", [] { return std::make_unique<OnePoleModule>(); });
    r.add("allpass1", [] { return std::make_unique<AllPass1Module>(); });
    r.add("allpass2", [] { return std::make_unique<AllPass2Module>(); });
    r.add("fir", [] { return std::make_unique<FirModule>(); });
    r.add("lr_crossover", [] { return std::make_unique<LrCrossoverModule>(); });
    r.add("envelope", [] { return std::make_unique<EnvelopeModule>(); });
    r.add("compressor_core", [] { return std::make_unique<CompressorCoreModule>(); });
    r.add("delay", [] { return std::make_unique<DelayModule>(); });
    r.add("oversampled_tanh", [] { return std::make_unique<OversampledTanhModule>(); });
}
} // namespace mmr
