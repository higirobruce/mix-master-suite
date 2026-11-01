#pragma once
#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"

namespace mm
{
// Detector pre-filter: 12 dB/oct Butterworth high-pass and/or low-pass (0 = off).
template <typename T> class SidechainFilter
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        setHighPass(hpHz);
        setLowPass(lpHz);
        reset();
    }
    void setHighPass(double hz) noexcept
    {
        hpHz = hz;
        hpOn = hz > 0.0;
        if (hpOn)
            hp.setCoeffs(BiquadCoeffs<T>::fromDouble(rbj::highPass(sr, hz, 0.70710678)));
    }
    void setLowPass(double hz) noexcept
    {
        lpHz = hz;
        lpOn = hz > 0.0;
        if (lpOn)
            lp.setCoeffs(BiquadCoeffs<T>::fromDouble(rbj::lowPass(sr, hz, 0.70710678)));
    }
    T process(T x) noexcept
    {
        if (hpOn)
            x = hp.process(x);
        if (lpOn)
            x = lp.process(x);
        return x;
    }
    void reset() noexcept
    {
        hp.reset();
        lp.reset();
    }

private:
    double sr = 48000.0, hpHz = 0.0, lpHz = 0.0;
    bool hpOn = false, lpOn = false;
    Biquad<T> hp, lp;
};
} // namespace mm
