#pragma once
// Uniform-partition overlap-save FFT convolution in double precision (ADR-0011). The impulse is cut into K
// partitions of P taps, each kept as its 2P-point spectrum; input is collected into a P-sample FIFO and, when it
// fills, the last 2P input samples are transformed once, multiplied-and-accumulated against the K partition spectra
// through a ring of the input's past spectra, inverse-transformed, and the last P samples become the next P outputs.
// Every sample is delayed by exactly P whatever the host's block schedule, plus the impulse's own centre:
// latencySamples() = P + (taps - 1) / 2 for a symmetric impulse. Sized in prepare(); process() and setImpulse()
// never allocate (setImpulse computes K transforms: callers keep it off the audio thread).
#include "core/fir/Fft.h"
#include <algorithm>
#include <vector>

namespace mm::fir
{
class PartitionedConvolver
{
public:
    // P = maxBlock rounded up to a power of two, at least minPartition (itself rounded up to a power of two, min 64)
    void prepare(int maxTaps, int maxBlock, int channels, int minPartition = 64)
    {
        P = 64;
        while (P < std::max(maxBlock, minPartition))
            P <<= 1;
        maxK = std::max(1, (std::max(1, maxTaps) + P - 1) / P);
        K = 0;
        taps = 0;
        fft.prepare(2 * P);
        const size_t spec = static_cast<size_t>(2 * P);
        hRe.assign(static_cast<size_t>(maxK) * spec, 0.0);
        hIm.assign(static_cast<size_t>(maxK) * spec, 0.0);
        workRe.assign(spec, 0.0);
        workIm.assign(spec, 0.0);
        accRe.assign(spec, 0.0);
        accIm.assign(spec, 0.0);
        chans.assign(static_cast<size_t>(std::max(1, channels)), Channel{});
        for (auto& c : chans)
        {
            c.inFifo.assign(static_cast<size_t>(P), 0.0);
            c.outFifo.assign(static_cast<size_t>(P), 0.0);
            c.prevBlock.assign(static_cast<size_t>(P), 0.0);
            c.histRe.assign(static_cast<size_t>(maxK) * spec, 0.0);
            c.histIm.assign(static_cast<size_t>(maxK) * spec, 0.0);
        }
        reset();
    }

    // n <= the prepared maxTaps. The convolution starts using the new partitions at the next block boundary.
    void setImpulse(const double* h, int n) noexcept
    {
        n = std::clamp(n, 0, maxK * P);
        taps = n;
        K = std::max(1, (n + P - 1) / P);
        const int spec = 2 * P;
        for (int j = 0; j < K; ++j)
        {
            std::fill(workRe.begin(), workRe.end(), 0.0);
            std::fill(workIm.begin(), workIm.end(), 0.0);
            const int from = j * P, count = std::min(P, n - from);
            for (int i = 0; i < count; ++i)
                workRe[static_cast<size_t>(i)] = h[from + i];
            fft.forward(workRe.data(), workIm.data());
            std::copy(workRe.begin(), workRe.end(), hRe.begin() + static_cast<std::ptrdiff_t>(j * spec));
            std::copy(workIm.begin(), workIm.end(), hIm.begin() + static_cast<std::ptrdiff_t>(j * spec));
        }
    }

    void process(float* buf, int n, int ch) noexcept
    {
        auto& c = chans[static_cast<size_t>(ch)];
        for (int i = 0; i < n; ++i)
        {
            c.inFifo[static_cast<size_t>(c.pos)] = static_cast<double>(buf[i]);
            buf[i] = static_cast<float>(c.outFifo[static_cast<size_t>(c.pos)]);
            if (++c.pos == P)
            {
                runBlock(c);
                c.pos = 0;
            }
        }
    }

    int latencySamples() const noexcept { return P + (std::max(taps, 1) - 1) / 2; }
    int partitionSize() const noexcept { return P; }
    int numTaps() const noexcept { return taps; }

    void reset() noexcept
    {
        for (auto& c : chans)
        {
            std::fill(c.inFifo.begin(), c.inFifo.end(), 0.0);
            std::fill(c.outFifo.begin(), c.outFifo.end(), 0.0);
            std::fill(c.prevBlock.begin(), c.prevBlock.end(), 0.0);
            std::fill(c.histRe.begin(), c.histRe.end(), 0.0);
            std::fill(c.histIm.begin(), c.histIm.end(), 0.0);
            c.pos = 0;
            c.head = 0;
        }
    }

private:
    struct Channel
    {
        std::vector<double> inFifo, outFifo, prevBlock, histRe, histIm;
        int pos = 0, head = 0; // head: the ring slot holding the newest input spectrum
    };

    void runBlock(Channel& c) noexcept
    {
        const int spec = 2 * P;
        // time block = previous P input samples followed by the P just collected (overlap-save)
        std::copy(c.prevBlock.begin(), c.prevBlock.end(), workRe.begin());
        std::copy(c.inFifo.begin(), c.inFifo.end(), workRe.begin() + P);
        std::fill(workIm.begin(), workIm.end(), 0.0);
        std::copy(c.inFifo.begin(), c.inFifo.end(), c.prevBlock.begin());
        fft.forward(workRe.data(), workIm.data());
        c.head = c.head == 0 ? maxK - 1 : c.head - 1;
        std::copy(workRe.begin(), workRe.end(), c.histRe.begin() + static_cast<std::ptrdiff_t>(c.head * spec));
        std::copy(workIm.begin(), workIm.end(), c.histIm.begin() + static_cast<std::ptrdiff_t>(c.head * spec));
        // acc = sum_j X[newest - j] * H[j]: separate real / imaginary arrays so the loops vectorise
        std::fill(accRe.begin(), accRe.end(), 0.0);
        std::fill(accIm.begin(), accIm.end(), 0.0);
        for (int j = 0; j < K; ++j)
        {
            int slot = c.head + j;
            if (slot >= maxK)
                slot -= maxK;
            const double* xr = c.histRe.data() + static_cast<std::ptrdiff_t>(slot * spec);
            const double* xi = c.histIm.data() + static_cast<std::ptrdiff_t>(slot * spec);
            const double* hr = hRe.data() + static_cast<std::ptrdiff_t>(j * spec);
            const double* hi = hIm.data() + static_cast<std::ptrdiff_t>(j * spec);
            double* ar = accRe.data();
            double* ai = accIm.data();
            for (int k = 0; k < spec; ++k)
            {
                ar[k] += xr[k] * hr[k] - xi[k] * hi[k];
                ai[k] += xr[k] * hi[k] + xi[k] * hr[k];
            }
        }
        fft.inverse(accRe.data(), accIm.data());
        std::copy(accRe.begin() + P, accRe.end(), c.outFifo.begin()); // the last P samples are free of wrap-around
    }

    int P = 64, maxK = 1, K = 0, taps = 0;
    Fft fft;
    std::vector<double> hRe, hIm, workRe, workIm, accRe, accIm;
    std::vector<Channel> chans;
};
} // namespace mm::fir
