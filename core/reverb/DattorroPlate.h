#pragma once
// Dattorro plate (J. Dattorro, "Effect Design Part 1", JAES 1997): input diffusers, two tank halves with modulated
// all-passes, damping and decay; output taps per the paper. Delay lengths scale from the paper's 29.761 kHz.
// RT60 (ADR-0008): four decay multipliers per loop of length L_loop, so decay = 10^(-3 L_loop / (4 RT60 fs)).
#include "core/reverb/ReverbParts.h"

namespace mm::reverb
{
template <typename T> class DattorroPlate
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        k = sr / 29761.0;
        auto L = [&](double n) { return std::max(1, static_cast<int>(std::lround(n * k))); };
        static const double inAp[4] = {142, 107, 379, 277};
        static const T inG[4] = {T(0.75), T(0.75), T(0.625), T(0.625)};
        for (int i = 0; i < 4; ++i)
        {
            inputAp[static_cast<size_t>(i)].prepare(L(inAp[i]) + 8);
            inputAp[static_cast<size_t>(i)].set(L(inAp[i]), inG[i]);
        }
        modAp[0].prepare(L(672 + 40));
        modAp[1].prepare(L(908 + 40));
        modAp[0].set(L(672), T(-0.7));
        modAp[1].set(L(908), T(-0.7));
        modLen[0] = static_cast<float>(672 * k);
        modLen[1] = static_cast<float>(908 * k);
        excursion = static_cast<float>(16.0 * k);
        delay1[0].prepare(L(4453) + 8);
        delay1[1].prepare(L(4217) + 8);
        len1[0] = L(4453);
        len1[1] = L(4217);
        ap2[0].prepare(L(1800) + 8);
        ap2[1].prepare(L(2656) + 8);
        ap2[0].set(L(1800), T(0.5));
        ap2[1].set(L(2656), T(0.5));
        delay2[0].prepare(L(3720) + 8);
        delay2[1].prepare(L(3163) + 8);
        len2[0] = L(3720);
        len2[1] = L(3163);
        loopSamples = 0;
        for (double n : {672.0, 4453.0, 1800.0, 3720.0, 908.0, 4217.0, 2656.0, 3163.0})
            loopSamples += L(n);
        lfoInc = 2.0 * std::numbers::pi * 1.0 / sr; // ~1 Hz tank modulation
        setRT60(rt60);
        setDamping(damping);
        reset();
    }
    // Labelled RT60 -> decay. The loop formula alone overshoots at short labels (the tank's onset is still filling
    // while the decay fit starts), so the label is first mapped through a measured correction table
    // (tools/calibrate_plate.py, Schroeder T60 at 48 kHz) and then through the loop formula.
    void setRT60(double seconds)
    {
        rt60 = std::max(0.05, seconds);
        setRT60Raw(correctedSeconds(rt60));
    }
    // formula only (calibration)
    void setRT60Raw(double seconds)
    {
        const double t = std::max(0.02, seconds);
        decay = static_cast<T>(std::pow(10.0, -3.0 * loopSamples / (4.0 * t * sr)));
    }
    static double correctedSeconds(double label) noexcept
    {
        // (label, formula seconds that measure as that label); linear interpolation in log-log
        // measured 2026-11-02 (tools/calibrate_plate.py). Below 0.5 s the plate cannot decay faster: its tank onset
        // (~0.36 s per half loop) sets a ~0.49 s floor, so labels under 0.5 s reach the floor.
        static const double tab[][2] = {{0.5, 0.2862}, {0.7, 0.5435}, {1.0, 0.8832}, {1.5, 1.4331}, {2.0, 1.9780},
                                        {3.0, 3.0331}, {5.0, 5.1839}, {8.0, 8.4256}, {12.0, 12.7581}, {20.0, 21.3976}};
        constexpr int n = static_cast<int>(sizeof(tab) / sizeof(tab[0]));
        if (label <= tab[0][0]) return tab[0][1] * label / tab[0][0];
        if (label >= tab[n - 1][0]) return tab[n - 1][1] * label / tab[n - 1][0];
        for (int i = 1; i < n; ++i)
            if (label <= tab[i][0])
            {
                const double u = std::log(label / tab[i - 1][0]) / std::log(tab[i][0] / tab[i - 1][0]);
                return std::exp(std::log(tab[i - 1][1]) + u * std::log(tab[i][1] / tab[i - 1][1]));
            }
        return label;
    }
    void setDamping(float d01)
    {
        damping = std::clamp(d01, 0.0f, 1.0f);
        const double fc = damping <= 0.0f ? sr : 20000.0 * std::pow(1000.0 / 20000.0, static_cast<double>(damping));
        damp[0].set(fc, sr);
        damp[1].set(fc, sr);
    }
    void setDiffusion(float d01)
    {
        const T g = static_cast<T>(0.5 + 0.25 * std::clamp(d01, 0.0f, 1.0f));
        auto L = [&](double n) { return std::max(1, static_cast<int>(std::lround(n * k))); };
        inputAp[0].set(L(142), g);
        inputAp[1].set(L(107), g);
        inputAp[2].set(L(379), g * T(0.83));
        inputAp[3].set(L(277), g * T(0.83));
    }

    void processStereo(T inL, T inR, T& outL, T& outR) noexcept
    {
        T x = T(0.5) * (inL + inR);
        for (auto& ap : inputAp)
            x = ap.process(x);
        const float m0 = modLen[0] + excursion * static_cast<float>(std::sin(lfo));
        const float m1 = modLen[1] + excursion * static_cast<float>(std::cos(lfo));
        lfo += lfoInc;
        if (lfo > 2.0 * std::numbers::pi)
            lfo -= 2.0 * std::numbers::pi;
        // half A fed by half B's tail, half B by half A's
        T a = modAp[0].processModulated(x + tailB, m0);
        delay1[0].write(a);
        a = damp[0].process(delay1[0].read(len1[0])) * decay;
        a = ap2[0].process(a);
        delay2[0].write(a);
        const T tailA = delay2[0].read(len2[0]) * decay;
        T b = modAp[1].processModulated(x + tailA, m1);
        delay1[1].write(b);
        b = damp[1].process(delay1[1].read(len1[1])) * decay;
        b = ap2[1].process(b);
        delay2[1].write(b);
        tailB = delay2[1].read(len2[1]) * decay;
        // output taps (paper, scaled)
        auto t = [&](double n) { return std::max(1, static_cast<int>(std::lround(n * k))); };
        outL = T(0.6) * (delay1[1].read(t(266)) + delay1[1].read(t(2974)) - ap2[1].tap(t(1913)) + delay2[1].read(t(1996))
                         - delay1[0].read(t(1990)) - ap2[0].tap(t(187)) - delay2[0].read(t(1066)));
        outR = T(0.6) * (delay1[0].read(t(353)) + delay1[0].read(t(3627)) - ap2[0].tap(t(1228)) + delay2[0].read(t(2673))
                         - delay1[1].read(t(2111)) - ap2[1].tap(t(335)) - delay2[1].read(t(121)));
    }
    void reset() noexcept
    {
        for (auto& ap : inputAp) ap.reset();
        for (auto& ap : modAp) ap.reset();
        for (auto& ap : ap2) ap.reset();
        for (auto& d : delay1) d.reset();
        for (auto& d : delay2) d.reset();
        for (auto& d : damp) d.reset();
        tailB = T(0);
        lfo = 0.0;
    }
    double loopSeconds() const noexcept { return loopSamples / sr; }

private:
    double sr = 48000.0, k = 1.0, rt60 = 2.0, lfo = 0.0, lfoInc = 0.0;
    float damping = 0.0f, excursion = 16.0f;
    float modLen[2] = {672.0f, 908.0f};
    int len1[2] = {4453, 4217}, len2[2] = {3720, 3163}, loopSamples = 21589;
    T decay = T(0.5), tailB = T(0);
    std::array<SchroederAllPass<T>, 4> inputAp;
    std::array<SchroederAllPass<T>, 2> modAp, ap2;
    std::array<RingDelay<T>, 2> delay1, delay2;
    std::array<OnePoleDamp<T>, 2> damp;
};
} // namespace mm::reverb
