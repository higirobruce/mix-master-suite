#include "plugins/lp_eq/LpEqProcessor.h"
#include "Params.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/MatchedBiquad.h"
#include "core/filters/Transfer.h"
#include "core/fir/LinearPhaseDesign.h"
#include "lp_eq_data.h"
#include "plugins/lp_eq/LpEqEditor.h"
#include <algorithm>
#include <cmath>

namespace lp = mm::plugins::lp_eq;

namespace
{
mm::PresetManager::FactorySource factory()
{
    mm::PresetManager::FactorySource f;
    f.getNamedResource = &lp_eq_data::getNamedResource;
    f.names = lp_eq_data::namedResourceList;
    f.count = lp_eq_data::namedResourceListSize;
    return f;
}
constexpr const char* kIds[LpEqProcessor::kBands][7] = {
    {lp::id::b1_enabled, lp::id::b1_type, lp::id::b1_freq_hz, lp::id::b1_gain_db, lp::id::b1_q, lp::id::b1_phase, lp::id::b1_ms},
    {lp::id::b2_enabled, lp::id::b2_type, lp::id::b2_freq_hz, lp::id::b2_gain_db, lp::id::b2_q, lp::id::b2_phase, lp::id::b2_ms},
    {lp::id::b3_enabled, lp::id::b3_type, lp::id::b3_freq_hz, lp::id::b3_gain_db, lp::id::b3_q, lp::id::b3_phase, lp::id::b3_ms},
    {lp::id::b4_enabled, lp::id::b4_type, lp::id::b4_freq_hz, lp::id::b4_gain_db, lp::id::b4_q, lp::id::b4_phase, lp::id::b4_ms},
    {lp::id::b5_enabled, lp::id::b5_type, lp::id::b5_freq_hz, lp::id::b5_gain_db, lp::id::b5_q, lp::id::b5_phase, lp::id::b5_ms},
    {lp::id::b6_enabled, lp::id::b6_type, lp::id::b6_freq_hz, lp::id::b6_gain_db, lp::id::b6_q, lp::id::b6_phase, lp::id::b6_ms},
    {lp::id::b7_enabled, lp::id::b7_type, lp::id::b7_freq_hz, lp::id::b7_gain_db, lp::id::b7_q, lp::id::b7_phase, lp::id::b7_ms},
    {lp::id::b8_enabled, lp::id::b8_type, lp::id::b8_freq_hz, lp::id::b8_gain_db, lp::id::b8_q, lp::id::b8_phase, lp::id::b8_ms},
};
} // namespace

LpEqProcessor::LpEqProcessor()
    : mm::PluginProcessorBase(lp::table(), lp::kPluginId,
                              BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true),
                              factory(), lp::kPluginName),
      juce::Thread("lp_eq design")
{
    for (int i = 0; i < kBands; ++i)
    {
        auto& b = bands[static_cast<size_t>(i)];
        b.enabled = paramRef(kIds[i][0]);
        b.type = paramRef(kIds[i][1]);
        b.freqRaw = paramRef(kIds[i][2]);
        b.gainRaw = paramRef(kIds[i][3]);
        b.qRaw = paramRef(kIds[i][4]);
        b.freq = &smoothed(kIds[i][2]);
        b.gain = &smoothed(kIds[i][3]);
        b.q = &smoothed(kIds[i][4]);
        b.phase = paramRef(kIds[i][5]);
        b.ms = paramRef(kIds[i][6]);
    }
    firLengthRef = paramRef(lp::id::fir_length);
}

LpEqProcessor::~LpEqProcessor() { stopThread(2000); }

juce::AudioProcessorEditor* LpEqProcessor::createEditor() { return new LpEqEditor(*this); }

// ---- parameter views

LpEqProcessor::BandView LpEqProcessor::band(int index) const noexcept
{
    const auto& b = bands[static_cast<size_t>(juce::jlimit(0, kBands - 1, index))];
    return {b.enabled.on(), static_cast<int>(std::lround(b.type.get())), static_cast<int>(std::lround(b.phase.get())),
            static_cast<int>(std::lround(b.ms.get())), static_cast<double>(b.freqRaw.get()), static_cast<double>(b.gainRaw.get()),
            static_cast<double>(b.qRaw.get())};
}

int LpEqProcessor::firLength() const noexcept { return kLengths[juce::jlimit(0, 3, static_cast<int>(std::lround(firLengthRef.get())))]; }

double LpEqProcessor::resolutionHz() const noexcept
{
    const double rate = getSampleRate() > 0.0 ? getSampleRate() : sr;
    return mm::fir::resolutionHz(firLength(), rate);
}

mm::BiquadCoeffs<double> LpEqProcessor::coeffsFor(int type, double rate, double freq, double gainDb, double q) noexcept
{
    const double f = juce::jlimit(1.0, 0.49 * rate, freq);
    switch (type)
    {
    case 1: return mm::matched::lowShelf(rate, f, gainDb, q);
    case 2: return mm::matched::highShelf(rate, f, gainDb, q);
    case 3: return mm::matched::highPass(rate, f, q);
    case 4: return mm::matched::lowPass(rate, f, q);
    case 5: return mm::rbj::notch(rate, f, q);
    default: return mm::matched::peak(rate, f, gainDb, q);
    }
}

std::complex<double> LpEqProcessor::staticResponseAt(double hz) const
{
    const double rate = getSampleRate() > 0.0 ? getSampleRate() : sr;
    std::complex<double> h = 1.0;
    for (int i = 0; i < kBands; ++i)
    {
        const auto v = band(i);
        if (!v.enabled)
            continue;
        const auto r = mm::evaluate(coeffsFor(v.type, rate, v.freq, v.gainDb, v.q), rate, hz);
        h *= v.phase == 1 ? std::complex<double>(std::abs(r), 0.0) : r;
    }
    return h;
}

// ---- lifecycle

void LpEqProcessor::prepareDsp(double sampleRate, int maxBlock, int channels)
{
    stopThread(2000);
    prepared.store(false);
    sr = sampleRate;
    convolver.prepare(16384, maxBlock, kLanes, kMinPartition);
    maxLatency = convolver.partitionSize() + (16384 - 2) / 2;
    for (auto& s : slots)
    {
        for (auto& sp : s.spectra)
            convolver.allocateSpectra(sp);
        s.state.store(0);
    }
    for (auto& lb : laneBuf)
        lb.assign(static_cast<size_t>(maxBlock), 0.0f);
    fadeSamples = std::max(1, static_cast<int>(std::lround(kFadeMs * 1e-3 * sr)));
    switchStep = 1.0 / std::max(1.0, kFadeMs * 1e-3 * sr);
    for (auto& b : bands)
    {
        b.lastFreq = -1.0;
        b.lastType = -1;
    }
    analyserPre.prepare(sr, 12, juce::jmin(channels, 2));
    analyserPost.prepare(sr, 12, juce::jmin(channels, 2));
    snapSmoothersToTargets();
    // initial impulse, synchronously, so the first block is right
    std::vector<double> scratchRe(static_cast<size_t>(2 * convolver.partitionSize())), scratchIm(scratchRe.size());
    lastRequested = snapshotRequest();
    designInto(lastRequested, slots[0], scratchRe, scratchIm);
    slots[0].state.store(3);
    activeSlot = 0;
    fadingSlot = -1;
    for (int lane = 0; lane < kLanes; ++lane)
        convolver.setSpectra(lane, &slots[0].spectra[static_cast<size_t>(lane)], 0);
    resetDsp();
    publishRequest(lastRequested);
    prepared.store(true);
    startThread();
}

void LpEqProcessor::resetDsp()
{
    convolver.reset();
    for (int lane = 0; lane < kLanes; ++lane)
        for (int i = 0; i < kBands; ++i)
        {
            auto& lb = laneBands[static_cast<size_t>(lane)][static_cast<size_t>(i)];
            lb.filter.reset();
            const auto v = band(i);
            const bool here = v.enabled && v.phase == 0 && (v.ms == 0 || v.ms == lane + 1);
            lb.onPos = lb.onTarget = here ? 1.0 : 0.0;
        }
    for (int i = 0; i < kBands; ++i)
        designMinimumPhase(i);
}

void LpEqProcessor::redesignForTests()
{
    snapSmoothersToTargets();
    const auto r = snapshotRequest();
    std::vector<double> scratchRe(static_cast<size_t>(2 * convolver.partitionSize())), scratchIm(scratchRe.size());
    int slot = (activeSlot + 1) % static_cast<int>(slots.size());
    while (slots[static_cast<size_t>(slot)].state.load() == 3)
        slot = (slot + 1) % static_cast<int>(slots.size());
    designInto(r, slots[static_cast<size_t>(slot)], scratchRe, scratchIm);
    slots[static_cast<size_t>(slot)].state.store(2);
    lastRequested = r;
    takeReadySlot();
    resetDsp();
}

// ---- the design request (audio thread -> worker)

LpEqProcessor::DesignRequest LpEqProcessor::snapshotRequest() const noexcept
{
    DesignRequest r;
    r.length = firLength();
    r.sampleRate = sr;
    for (int i = 0; i < kBands; ++i)
    {
        const auto v = band(i);
        auto& rb = r.bands[static_cast<size_t>(i)];
        rb.enabled = v.enabled && v.phase == 1;
        rb.type = v.type;
        rb.ms = v.ms;
        rb.freq = v.freq;
        rb.gainDb = v.gainDb;
        rb.q = v.q;
    }
    return r;
}

void LpEqProcessor::publishRequest(const DesignRequest& r) noexcept
{
    requestSeq.fetch_add(1, std::memory_order_acq_rel); // odd: writing
    request = r;
    requestSeq.fetch_add(1, std::memory_order_acq_rel); // even: consistent
}

bool LpEqProcessor::readRequest(DesignRequest& r) const noexcept
{
    const unsigned a = requestSeq.load(std::memory_order_acquire);
    if (a & 1u)
        return false;
    r = request;
    return requestSeq.load(std::memory_order_acquire) == a;
}

void LpEqProcessor::designInto(const DesignRequest& r, Slot& slot, std::vector<double>& scratchRe, std::vector<double>& scratchIm)
{
    for (int lane = 0; lane < kLanes; ++lane)
    {
        std::vector<mm::BiquadCoeffs<double>> coeffs;
        for (const auto& b : r.bands)
            if (b.enabled && (b.ms == 0 || b.ms == lane + 1))
                coeffs.push_back(coeffsFor(b.type, r.sampleRate, b.freq, b.gainDb, b.q));
        const auto h = mm::fir::design(coeffs, r.length, r.sampleRate);
        convolver.computeSpectraInto(h.data(), static_cast<int>(h.size()), slot.spectra[static_cast<size_t>(lane)], scratchRe.data(), scratchIm.data());
    }
}

void LpEqProcessor::run()
{
    std::vector<double> scratchRe(static_cast<size_t>(2 * convolver.partitionSize())), scratchIm(scratchRe.size());
    DesignRequest done = lastRequested, r;
    while (!threadShouldExit())
    {
        if (!prepared.load() || !readRequest(r) || r == done)
        {
            wait(5);
            continue;
        }
        int slot = -1;
        for (int i = 0; i < static_cast<int>(slots.size()); ++i)
        {
            int expected = 0;
            if (slots[static_cast<size_t>(i)].state.compare_exchange_strong(expected, 1))
            {
                slot = i;
                break;
            }
        }
        if (slot < 0)
        {
            wait(5); // a ready slot is waiting to be taken; try again
            continue;
        }
        designInto(r, slots[static_cast<size_t>(slot)], scratchRe, scratchIm);
        // retire any older ready slot the audio thread has not taken yet (this one supersedes it)
        for (int i = 0; i < static_cast<int>(slots.size()); ++i)
        {
            int expected = 2;
            if (i != slot)
                slots[static_cast<size_t>(i)].state.compare_exchange_strong(expected, 0);
        }
        slots[static_cast<size_t>(slot)].state.store(2);
        done = r;
        wait(static_cast<int>(kDesignHoldMs)); // at most ~20 designs per second during a drag
    }
}

void LpEqProcessor::takeReadySlot() noexcept
{
    for (int i = 0; i < static_cast<int>(slots.size()); ++i)
    {
        auto& s = slots[static_cast<size_t>(i)];
        int expected = 2;
        if (!s.state.compare_exchange_strong(expected, 3))
            continue;
        if (fadingSlot >= 0 && fadingSlot != i)
            slots[static_cast<size_t>(fadingSlot)].state.store(0); // its fade is over (a new swap ends it)
        fadingSlot = activeSlot;
        activeSlot = i;
        for (int lane = 0; lane < kLanes; ++lane)
            convolver.setSpectra(lane, &s.spectra[static_cast<size_t>(lane)], fadeSamples);
        designCounter = 0;
        return;
    }
    // free the fading slot once its fade has run out
    if (fadingSlot >= 0 && designCounter > fadeSamples + convolver.partitionSize())
    {
        slots[static_cast<size_t>(fadingSlot)].state.store(0);
        fadingSlot = -1;
    }
}

// ---- per block

void LpEqProcessor::beginBlock(int numSamples)
{
    takeReadySlot();
    designCounter += numSamples;
    const auto r = snapshotRequest();
    if (!(r == lastRequested))
    {
        lastRequested = r;
        publishRequest(r);
    }
}

void LpEqProcessor::designMinimumPhase(int index) noexcept
{
    auto& b = bands[static_cast<size_t>(index)];
    const double f = b.freq->current(), g = b.gain->current(), q = b.q->current();
    const int type = static_cast<int>(std::lround(b.type.get()));
    if (f == b.lastFreq && g == b.lastGain && q == b.lastQ && type == b.lastType)
        return;
    b.lastFreq = f;
    b.lastGain = g;
    b.lastQ = q;
    b.lastType = type;
    b.coeffs = coeffsFor(type, sr, f, g, q);
    for (auto& lane : laneBands)
        lane[static_cast<size_t>(index)].filter.setCoeffs(b.coeffs);
}

void LpEqProcessor::processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int numMain = juce::jmin(getMainBusNumInputChannels(), buffer.getNumChannels(), 2);
    const int numOut = juce::jmin(getMainBusNumOutputChannels(), buffer.getNumChannels(), 2);
    if (numMain <= 0 || numOut <= 0)
        return;
    analyserPre.push(buffer.getArrayOfReadPointers(), numMain, n);
    for (auto& b : bands)
        for (auto* s : {b.freq, b.gain, b.q})
            s->skip(n);
    // where each minimum-phase band belongs this block (targets; the position ramps per sample)
    for (int i = 0; i < kBands; ++i)
    {
        const auto v = band(i);
        for (int lane = 0; lane < kLanes; ++lane)
            laneBands[static_cast<size_t>(lane)][static_cast<size_t>(i)].onTarget = (v.enabled && v.phase == 0 && (v.ms == 0 || v.ms == lane + 1)) ? 1.0 : 0.0;
    }
    float* l = buffer.getWritePointer(0);
    float* rPtr = numMain > 1 ? buffer.getWritePointer(1) : nullptr;
    float* mid = laneBuf[0].data();
    float* side = laneBuf[1].data();
    for (int i = 0; i < n; ++i)
    {
        const double L = l[i], R = rPtr != nullptr ? rPtr[i] : L;
        double lanes[kLanes] = {0.5 * (L + R), 0.5 * (L - R)};
        if ((modCounter++ & 31) == 0)
            for (int b = 0; b < kBands; ++b)
                designMinimumPhase(b);
        for (int lane = 0; lane < kLanes; ++lane)
        {
            double x = lanes[lane];
            for (int b = 0; b < kBands; ++b)
            {
                auto& lb = laneBands[static_cast<size_t>(lane)][static_cast<size_t>(b)];
                if (lb.onPos != lb.onTarget)
                    lb.onPos = lb.onPos < lb.onTarget ? std::min(lb.onTarget, lb.onPos + switchStep) : std::max(lb.onTarget, lb.onPos - switchStep);
                if (lb.onPos > 0.0)
                {
                    const double y = lb.filter.process(x);
                    x += (y - x) * lb.onPos;
                }
                else
                    lb.filter.reset(); // a band that is out keeps no state: it joins from silence
            }
            lanes[lane] = x;
        }
        mid[i] = static_cast<float>(lanes[0]);
        side[i] = static_cast<float>(lanes[1]);
    }
    convolver.process(mid, n, 0);
    if (rPtr != nullptr)
        convolver.process(side, n, 1);
    for (int i = 0; i < n; ++i)
    {
        const double M = mid[i], S = rPtr != nullptr ? side[i] : 0.0;
        l[i] = static_cast<float>(M + S);
        if (rPtr != nullptr)
            rPtr[i] = static_cast<float>(M - S);
    }
    analyserPost.push(buffer.getArrayOfReadPointers(), numMain, n);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LpEqProcessor(); }
