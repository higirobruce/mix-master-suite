// mm_bench_core: ns per sample per core module at 48 kHz (1 s of audio, best of 5 runs). Prints JSON.
#include "core/common/Math.h"
#include "core/delay/FractionalDelayLine.h"
#include "core/dynamics/Detector.h"
#include "core/dynamics/EnvelopeFollower.h"
#include "core/dynamics/GainComputer.h"
#include "core/dynamics/Lookahead.h"
#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/FIRDesign.h"
#include "core/filters/FIRFilter.h"
#include "core/filters/LinkwitzRiley.h"
#include "core/filters/SVF.h"
#include "core/metering/Correlation.h"
#include "core/metering/RmsMeter.h"
#include "core/nonlinear/ADAA.h"
#include "core/nonlinear/ADAATable.h"
#include "core/oversampling/Oversampler.h"
#include "core/nonlinear/TapeHysteresis.h"
#include "core/nonlinear/TubeTransfer.h"
#include "core/nonlinear/Waveshapers.h"
#include "core/smoothing/Smoother.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
constexpr double kSr = 48000.0;
constexpr int kN = 48000;

template <typename Fn> double nsPerSample(Fn&& fn, const std::vector<float>& x)
{
    double best = 1e18;
    volatile float sink = 0.0f;
    for (int rep = 0; rep < 5; ++rep)
    {
        const auto t0 = std::chrono::steady_clock::now();
        float acc = 0.0f;
        for (float v : x)
            acc += fn(v);
        sink = acc;
        const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / static_cast<double>(x.size());
        best = std::min(best, ns);
    }
    (void) sink;
    return best;
}
} // namespace

int main()
{
    std::vector<float> x(kN);
    unsigned seed = 5;
    for (auto& v : x)
    {
        seed = seed * 1664525u + 1013904223u;
        v = 0.25f * ((static_cast<float>(seed >> 8) / 8388608.0f) - 1.0f);
    }
    std::vector<std::pair<const char*, double>> rows;

    mm::Biquad<float> bq;
    bq.setCoeffs(mm::BiquadCoeffs<float>::fromDouble(mm::rbj::peak(kSr, 1000.0, 6.0, 1.0)));
    rows.push_back({"biquad_float", nsPerSample([&](float v) { return bq.process(v); }, x)});
    mm::Biquad<double> bqd;
    bqd.setCoeffs(mm::rbj::peak(kSr, 1000.0, 6.0, 1.0));
    rows.push_back({"biquad_double", nsPerSample([&](float v) { return static_cast<float>(bqd.process(v)); }, x)});
    mm::SVF<float> svf;
    svf.prepare(kSr);
    svf.setMode(mm::SvfMode::Peak);
    svf.setParams(1000.0, 1.0, 6.0);
    rows.push_back({"svf_float", nsPerSample([&](float v) { return svf.process(v); }, x)});
    mm::LinkwitzRiley<double> lr;
    lr.prepare(kSr);
    lr.setOrder(mm::LrOrder::LR4);
    lr.setCrossover(1000.0);
    rows.push_back({"lr4_double", nsPerSample([&](float v) { double lo = 0.0, hi = 0.0; lr.process(v, lo, hi); return static_cast<float>(lo + hi); }, x)});
    mm::FIRFilter<float> fir;
    fir.prepare(255, 1);
    auto taps = mm::fir::windowedSinc(255, 0.25, mm::fir::Window::Kaiser);
    fir.setTaps(taps.data(), 255);
    rows.push_back({"fir_255_float", nsPerSample([&](float v) { return fir.process(v, 0); }, x)});
    mm::Smoother<float> sm;
    sm.prepare(kSr, 20.0);
    sm.setTarget(1.0f);
    rows.push_back({"smoother", nsPerSample([&](float v) { return sm.next() * v; }, x)});
    mm::Detector<float> det;
    det.prepare(kSr);
    det.setMode(mm::DetectorMode::Rms);
    mm::EnvelopeFollower<float> env;
    env.prepare(kSr);
    mm::GainComputer<float> gc;
    gc.set({-20.0, 4.0, 6.0, false, -80.0});
    rows.push_back({"compressor_chain", nsPerSample([&](float v) {
        const float r = -gc.gainDb(mm::gainToDb(det.process(v)));
        return v * mm::dbToGain(-env.process(r));
    }, x)});
    mm::Lookahead<float> la;
    la.prepare(kSr, 10.0, 1);
    la.setMs(5.0);
    rows.push_back({"lookahead_5ms", nsPerSample([&](float v) { return la.maxEnvelope(std::abs(v)) + la.delaySignal(v, 0); }, x)});
    mm::FractionalDelayLine<float> dl;
    dl.prepare(kSr, 1.0, 1);
    dl.setDelaySamples(1234.5f, 0);
    rows.push_back({"fractional_delay_lagrange3", nsPerSample([&](float v) { dl.push(v, 0); return dl.read(0); }, x)});
    rows.push_back({"tanh_adaa", nsPerSample([&, adaa = mm::ADAA1<mm::shape::tanhS, mm::shape::tanhAD1D>()](float v) mutable { return adaa.process(v * 4.0f); }, x)});
    mm::TapeHysteresis tape;
    tape.prepare(kSr * 4);
    tape.set(0.5, 0.5, 0.5);
    rows.push_back({"tape_hysteresis", nsPerSample([&](float v) { return tape.process(v); }, x)});
    mm::TubeTransfer tube;
    tube.set(1.0, 0.5);
    rows.push_back({"tube", nsPerSample([&](float v) { return tube.process(v); }, x)});
    mm::RmsMeter rms;
    rms.prepare(kSr);
    rows.push_back({"rms_meter", nsPerSample([&](float v) { rms.process(&v, 1); return v; }, x)});
    mm::CorrelationMeter corr;
    corr.prepare(kSr);
    rows.push_back({"correlation", nsPerSample([&](float v) { corr.process(&v, &v, 1); return v; }, x)});
    rows.push_back({"tanh_adaa_table", nsPerSample([&, t = mm::TabulatedADAA1<mm::shape::tanhS, mm::shape::tanhAD1D>()](float v) mutable { return t.process(v * 4.0f); }, x)});
    rows.push_back({"atan_adaa_table", nsPerSample([&, t = mm::TabulatedADAA1<mm::shape::atanS, mm::shape::atanAD1D>()](float v) mutable { return t.process(v * 4.0f); }, x)});
    // oversampler round trip (up + down, no processing) per host frame, stereo, 64-sample blocks
    for (int factor : {2, 4, 8})
    {
        mm::Oversampler os;
        os.prepare(kSr, 64, 2, factor);
        juce::AudioBuffer<float> blk(2, 64);
        int pos = 0;
        auto fn = [&](float v) {
            blk.setSample(0, pos, v);
            blk.setSample(1, pos, -v);
            if (++pos == 64)
            {
                pos = 0;
                juce::dsp::AudioBlock<float> b(blk);
                os.up(b);
                os.down(b);
            }
            return blk.getSample(0, pos);
        };
        static const char* names[] = {"oversampler_2x_stereo_frame", "oversampler_4x_stereo_frame", "oversampler_8x_stereo_frame"};
        rows.push_back({names[factor == 2 ? 0 : (factor == 4 ? 1 : 2)], nsPerSample(fn, x)});
    }

    std::printf("{\n");
    for (size_t i = 0; i < rows.size(); ++i)
        std::printf("  \"%s\": %.2f%s\n", rows[i].first, rows[i].second, i + 1 < rows.size() ? "," : "");
    std::printf("}\n");
    return 0;
}
