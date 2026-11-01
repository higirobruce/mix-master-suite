#pragma once
#include <cmath>

namespace mm
{
// Normalised biquad coefficients (a0 = 1): y = b0 x + b1 x1 + b2 x2 - a1 y1 - a2 y2.
template <typename T> struct BiquadCoeffs
{
    T b0 = T(1), b1 = T(0), b2 = T(0), a1 = T(0), a2 = T(0);

    template <typename U> static BiquadCoeffs<T> fromDouble(const BiquadCoeffs<U>& c) noexcept
    {
        return {static_cast<T>(c.b0), static_cast<T>(c.b1), static_cast<T>(c.b2), static_cast<T>(c.a1), static_cast<T>(c.a2)};
    }
};

// Transposed direct form II. Coefficients may change every sample (TDF-II state is well behaved under modulation
// for the filters used here; the SVF is the choice for heavily modulated resonant filters).
template <typename T> class Biquad
{
public:
    void setCoeffs(const BiquadCoeffs<T>& newCoeffs) noexcept { c = newCoeffs; }
    const BiquadCoeffs<T>& coeffs() const noexcept { return c; }

    T process(T x) noexcept
    {
        const T y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void process(T* buf, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            buf[i] = process(buf[i]);
    }
    void reset() noexcept { s1 = s2 = T(0); }

private:
    BiquadCoeffs<T> c;
    T s1{}, s2{};
};
} // namespace mm
