#pragma once
// Linkwitz-Riley crossover: squared Butterworth sections so low + high sums to an all-pass (phase coherent).
// LR2 = (1st-order)^2 with the high band inverted; LR4 = (2nd-order Butterworth)^2; LR8 = (4th-order Butterworth)^2.
#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/OnePole.h"
#include "core/filters/Transfer.h"
#include <array>
#include <complex>

namespace mm
{
enum class LrOrder
{
    LR2 = 2,
    LR4 = 4,
    LR8 = 8
};

template <typename T> class LinkwitzRiley
{
public:
    using Order = LrOrder;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& p : lp1) p.prepare(sr);
        for (auto& p : hp1) p.prepare(sr);
        design();
        reset();
    }
    void setOrder(Order o) noexcept
    {
        order = o;
        design();
    }
    void setCrossover(double hz) noexcept
    {
        fc = hz;
        design();
    }

    void process(T x, T& low, T& high) noexcept
    {
        low = high = T(0);
        switch (order)
        {
        case Order::LR2:
            low = lp1[1].process(lp1[0].process(x));
            high = -hp1[1].process(hp1[0].process(x));
            break;
        case Order::LR4:
            low = lpB[1].process(lpB[0].process(x));
            high = hpB[1].process(hpB[0].process(x));
            break;
        case Order::LR8:
            low = lpB[3].process(lpB[2].process(lpB[1].process(lpB[0].process(x))));
            high = hpB[3].process(hpB[2].process(hpB[1].process(hpB[0].process(x))));
            break;
        }
    }
    void reset() noexcept
    {
        for (auto& p : lp1) p.reset();
        for (auto& p : hp1) p.reset();
        for (auto& b : lpB) b.reset();
        for (auto& b : hpB) b.reset();
    }

    std::complex<double> evaluateLow(double hz) const noexcept
    {
        if (order == Order::LR2)
            return lp1[0].evaluate(hz) * lp1[1].evaluate(hz);
        std::complex<double> h = 1.0;
        for (int i = 0; i < numBiquads(); ++i)
            h *= evaluate(lpCoeffs[static_cast<size_t>(i)], sr, hz);
        return h;
    }
    std::complex<double> evaluateHigh(double hz) const noexcept
    {
        if (order == Order::LR2)
            return -hp1[0].evaluate(hz) * hp1[1].evaluate(hz);
        std::complex<double> h = 1.0;
        for (int i = 0; i < numBiquads(); ++i)
            h *= evaluate(hpCoeffs[static_cast<size_t>(i)], sr, hz);
        return h;
    }
    Order getOrder() const noexcept { return order; }

private:
    int numBiquads() const noexcept { return order == Order::LR4 ? 2 : (order == Order::LR8 ? 4 : 0); }
    void design() noexcept
    {
        for (auto& p : lp1) p.setCutoff(fc);
        for (auto& p : hp1) p.setCutoff(fc);
        // Butterworth Q values: 2nd order {0.7071}; 4th order {0.5412, 1.3066}; each squared for LR
        const double q4[2] = {0.54119610, 1.30656296};
        for (int i = 0; i < 4; ++i)
        {
            const double q = order == Order::LR4 ? 0.70710678 : q4[i / 2];
            lpCoeffs[static_cast<size_t>(i)] = rbj::lowPass(sr, fc, q);
            hpCoeffs[static_cast<size_t>(i)] = rbj::highPass(sr, fc, q);
            lpB[static_cast<size_t>(i)].setCoeffs(BiquadCoeffs<T>::fromDouble(lpCoeffs[static_cast<size_t>(i)]));
            hpB[static_cast<size_t>(i)].setCoeffs(BiquadCoeffs<T>::fromDouble(hpCoeffs[static_cast<size_t>(i)]));
        }
    }

    double sr = 48000.0, fc = 1000.0;
    Order order = Order::LR4;
    std::array<OnePoleLP<T>, 2> lp1;
    std::array<OnePoleHP<T>, 2> hp1;
    std::array<Biquad<T>, 4> lpB, hpB;
    std::array<BiquadCoeffs<double>, 4> lpCoeffs, hpCoeffs;
};
} // namespace mm
