#include "plugins/delay/DelayProcessor.h"
#include "Params.h"
#include "core/common/Math.h"
#include "core/delay/TempoSync.h"
#include "core/nonlinear/Waveshapers.h"
#include "delay_data.h"
#include "plugins/delay/DelayEditor.h"
#include <cmath>
#include <numbers>

namespace dp = mm::plugins::delay;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &delay_data::getNamedResource;
    f.names = delay_data::namedResourceList;
    f.count = delay_data::namedResourceListSize;
    return f;
}
// sync choices: Off, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4 dotted, 1/8 dotted, 1/16 dotted, 1/4 triplet, 1/8 triplet, 1/16 triplet
struct Note
{
    int division;
    bool dotted, triplet;
};
const Note kNotes[13] = {{0, false, false}, {1, false, false}, {2, false, false}, {4, false, false}, {8, false, false},
                         {16, false, false}, {32, false, false}, {4, true, false}, {8, true, false}, {16, true, false},
                         {4, false, true}, {8, false, true}, {16, false, true}};
// feedback soft limit: linear to 0 dBFS, quadratic knee to +6 dBFS, flat beyond (ADR-0007)
inline float softLimit(float x) noexcept { return 2.0f * mm::shape::softClip(0.5f * x, 0.5f); }
} // namespace

DelayProcessor::DelayProcessor()
    : mm::PluginProcessorBase(dp::table(), dp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), dp::kPluginName)
{
    timeL = paramRef(dp::id::time_l_ms);
    timeR = paramRef(dp::id::time_r_ms);
    linkLr = paramRef(dp::id::link_lr);
    sync = paramRef(dp::id::sync);
    pingPong = paramRef(dp::id::ping_pong);
    feedback = &smoothed(dp::id::feedback_pct);
    fbLp = &smoothed(dp::id::fb_lp_hz);
    fbHp = &smoothed(dp::id::fb_hp_hz);
    modDepth = &smoothed(dp::id::mod_depth_ms);
    modRate = &smoothed(dp::id::mod_rate_hz);
    mix = &smoothed(dp::id::mix_pct);
    duckAmount = &smoothed(dp::id::duck_amount_db);
    duckRelease = &smoothed(dp::id::duck_release_ms);
    for (auto& e : effectiveMs)
        e.store(375.0f);
}

void DelayProcessor::prepareDsp(double sampleRate, int, int)
{
    sr = sampleRate;
    line.prepare(sr, kMaxDelaySeconds + 0.1, kChannels);
    line.setInterp(mm::DelayInterp::Lagrange3);
    line.setSlewMs(0.0); // the processor owns slew and crossfade
    for (auto& f : lp)
    {
        f.prepare(sr);
        f.setMode(mm::SvfMode::LowPass);
    }
    for (auto& f : hp)
    {
        f.prepare(sr);
        f.setMode(mm::SvfMode::HighPass);
    }
    duckEnv.prepare(sr);
    duckEnv.setAttackMs(5.0);
    xfStep = 1.0 / juce::jmax(1.0, kCrossfadeMs * 1e-3 * sr);
    resetDsp();
    for (int ch = 0; ch < kChannels; ++ch)
    {
        const double ms = ch == 0 ? timeL.get() : timeR.get();
        time[static_cast<size_t>(ch)].current = time[static_cast<size_t>(ch)].target = juce::jlimit(1.0, kMaxDelaySeconds * sr, ms * 1e-3 * sr);
        time[static_cast<size_t>(ch)].xfPos = 1.0;
    }
}

void DelayProcessor::resetDsp()
{
    line.reset();
    for (auto& f : lp)
        f.reset();
    for (auto& f : hp)
        f.reset();
    lastWet = {0.0f, 0.0f};
    duckEnv.reset();
    duckEnv.snapTo(-120.0f);
    lfoPhase = 0.0;
}

void DelayProcessor::beginBlock(int)
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 1.0)
                    bpm.store(*b, std::memory_order_relaxed);
}

void DelayProcessor::setTargetSamples(int ch, double samples) noexcept
{
    auto& t = time[static_cast<size_t>(ch)];
    samples = juce::jlimit(1.0, kMaxDelaySeconds * sr, samples);
    if (std::abs(samples - t.target) < 0.01)
        return;
    t.target = samples;
    if (std::abs(samples - t.current) > kGlideLimitMs * 1e-3 * sr)
    {
        // large jump: crossfade from the tap we are hearing to the new one (a slew would replay the buffer at speed)
        t.xfOld = t.current;
        t.xfPos = 0.0;
        t.current = samples;
        t.increment = 0.0;
    }
    else
        t.increment = (samples - t.current) / juce::jmax(1.0, kSlewMs * 1e-3 * sr); // small move: pitch glide
}

void DelayProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numIn = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels());
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), kChannels);
    if (numIn <= 0 || numOut <= 0)
        return;
    const float mixStart = mix->current() * 0.01f;
    for (auto* s : {feedback, fbLp, fbHp, modDepth, modRate, mix, duckAmount, duckRelease})
        s->skip(n);
    // times: sync from the play head, else the knobs (R follows L when linked)
    const int syncIdx = juce::jlimit(0, 12, static_cast<int>(std::lround(sync.get())));
    double msL = timeL.get(), msR = linkLr.on() ? msL : timeR.get();
    if (syncIdx > 0)
    {
        const auto& nt = kNotes[syncIdx];
        msL = msR = juce::jlimit(1.0, kMaxDelaySeconds * 1000.0, mm::TempoSync::noteMs(currentBpm(), nt.division, nt.dotted, nt.triplet));
    }
    effectiveMs[0].store(static_cast<float>(msL), std::memory_order_relaxed);
    effectiveMs[1].store(static_cast<float>(msR), std::memory_order_relaxed);
    setTargetSamples(0, msL * 1e-3 * sr);
    setTargetSamples(1, msR * 1e-3 * sr);

    const float fb = feedback->current() * 0.01f;
    const bool pp = pingPong.on();
    const bool lpOn = fbLp->current() < 19999.0f, hpOn = fbHp->current() > 20.5f;
    for (int ch = 0; ch < kChannels; ++ch)
    {
        lp[static_cast<size_t>(ch)].setParams(fbLp->current(), 0.7071, 0.0);
        hp[static_cast<size_t>(ch)].setParams(fbHp->current(), 0.7071, 0.0);
    }
    const float depthSamples = modDepth->current() * 1e-3f * static_cast<float>(sr);
    const double lfoInc = 2.0 * std::numbers::pi * modRate->current() / sr;
    duckEnv.setReleaseMs(duckRelease->current());
    const float duckDb = duckAmount->current();
    const float mixEnd = mix->current() * 0.01f;

    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float>(i + 1) / static_cast<float>(n);
        const float mixAmt = mixStart + (mixEnd - mixStart) * t;
        const float inL = buffer.getSample(0, i);
        const float inR = numIn > 1 ? buffer.getSample(1, i) : inL;
        // feedback from the last wet outputs (crossed for ping-pong), filtered and soft-limited
        float fbIn[kChannels] = {lastWet[pp ? 1 : 0] * fb, lastWet[pp ? 0 : 1] * fb};
        for (int ch = 0; ch < kChannels; ++ch)
        {
            float v = fbIn[ch];
            if (lpOn) v = lp[static_cast<size_t>(ch)].process(v);
            if (hpOn) v = hp[static_cast<size_t>(ch)].process(v);
            fbIn[ch] = v;
        }
        const float srcL = pp ? 0.5f * (inL + inR) : inL;
        const float srcR = pp ? 0.0f : inR;
        line.push(softLimit(srcL + fbIn[0]), 0); // the whole line input is bounded at +6 dBFS (ADR-0007)
        line.push(softLimit(srcR + fbIn[1]), 1);
        // LFO (R at 90 degrees)
        const float lfo0 = depthSamples > 0.0f ? depthSamples * static_cast<float>(std::sin(lfoPhase)) : 0.0f;
        const float lfo1 = depthSamples > 0.0f ? depthSamples * static_cast<float>(std::cos(lfoPhase)) : 0.0f;
        lfoPhase += lfoInc;
        if (lfoPhase > 2.0 * std::numbers::pi)
            lfoPhase -= 2.0 * std::numbers::pi;
        float wet[kChannels];
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto& ts = time[static_cast<size_t>(ch)];
            if (ts.increment != 0.0)
            {
                ts.current += ts.increment;
                if ((ts.increment > 0.0 && ts.current >= ts.target) || (ts.increment < 0.0 && ts.current <= ts.target))
                {
                    ts.current = ts.target;
                    ts.increment = 0.0;
                }
            }
            const float mod = ch == 0 ? lfo0 : lfo1;
            const float d = juce::jmax(1.0f, static_cast<float>(ts.current) + mod);
            float v = line.readAt(d, ch);
            if (ts.xfPos < 1.0)
            {
                const float old = line.readAt(juce::jmax(1.0f, static_cast<float>(ts.xfOld) + mod), ch);
                const float a = static_cast<float>(ts.xfPos * std::numbers::pi * 0.5);
                v = old * std::cos(a) + v * std::sin(a); // equal-power
                ts.xfPos = juce::jmin(1.0, ts.xfPos + xfStep);
            }
            wet[ch] = v;
        }
        lastWet = {wet[0], wet[1]};
        // ducking keyed from the dry: full duck above -20 dBFS, none below -40
        float duckGain = 1.0f;
        if (duckDb > 0.0f)
        {
            const float env = duckEnv.process(mm::fastGainToDb(juce::jmax(std::abs(inL), std::abs(inR))));
            const float amount = juce::jlimit(0.0f, 1.0f, (env + 40.0f) / 20.0f);
            duckGain = mm::fastDbToGain(-duckDb * amount);
        }
        for (int ch = 0; ch < numOut; ++ch)
        {
            const float dry = ch == 0 ? inL : inR;
            buffer.setSample(ch, i, dry * (1.0f - mixAmt) + wet[ch] * mixAmt * duckGain);
        }
    }
}

juce::AudioProcessorEditor* DelayProcessor::createEditor() { return new DelayEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DelayProcessor(); }
