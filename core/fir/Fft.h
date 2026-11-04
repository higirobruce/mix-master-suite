#pragma once
// Radix-2 complex FFT in double precision (JUCE's FFT is float only; the linear-phase engine is 64-bit, ADR-0011).
// Iterative decimation-in-time with precomputed twiddles and bit-reversal table; sized in prepare(), the transforms
// never allocate. Not the fastest FFT there is: the engine's cost is dominated by the partition multiply-accumulate.
#include <cmath>
#include <numbers>
#include <vector>

namespace mm::fir
{
class Fft
{
public:
    void prepare(int size)
    {
        n = 1;
        log2n = 0;
        while (n < size)
        {
            n <<= 1;
            ++log2n;
        }
        cosT.assign(static_cast<size_t>(n / 2 + 1), 0.0);
        sinT.assign(static_cast<size_t>(n / 2 + 1), 0.0);
        for (int k = 0; k <= n / 2; ++k)
        {
            const double a = -2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(n);
            cosT[static_cast<size_t>(k)] = std::cos(a);
            sinT[static_cast<size_t>(k)] = std::sin(a);
        }
        rev.assign(static_cast<size_t>(n), 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < log2n; ++b)
                r |= ((i >> b) & 1) << (log2n - 1 - b);
            rev[static_cast<size_t>(i)] = r;
        }
    }
    int size() const noexcept { return n; }

    void forward(double* re, double* im) const noexcept { transform(re, im, false); }
    // inverse, scaled by 1 / n
    void inverse(double* re, double* im) const noexcept
    {
        transform(re, im, true);
        const double s = 1.0 / static_cast<double>(n);
        for (int i = 0; i < n; ++i)
        {
            re[i] *= s;
            im[i] *= s;
        }
    }

private:
    void transform(double* re, double* im, bool inv) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[static_cast<size_t>(i)];
            if (j > i)
            {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int start = 0; start < n; start += len)
                for (int k = 0; k < half; ++k)
                {
                    const double wr = cosT[static_cast<size_t>(k * step)];
                    const double wi = inv ? -sinT[static_cast<size_t>(k * step)] : sinT[static_cast<size_t>(k * step)];
                    const int a = start + k, b = a + half;
                    const double tr = re[b] * wr - im[b] * wi;
                    const double ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr;
                    im[b] = im[a] - ti;
                    re[a] += tr;
                    im[a] += ti;
                }
        }
    }

    int n = 0, log2n = 0;
    std::vector<double> cosT, sinT;
    std::vector<int> rev;
};
} // namespace mm::fir
