#pragma once
// Uniform-partition overlap-save FFT convolution in double precision (ADR-0011). The impulse is cut into K
// partitions of P taps, each kept as its 2P-point spectrum (a `Spectra`); input is collected into a P-sample FIFO
// and, when it fills, the last 2P input samples are transformed once, multiplied-and-accumulated against the K
// partition spectra through a ring of the input's past spectra, inverse-transformed, and the last P samples become
// the next P outputs. Every sample is delayed by exactly P whatever the host's block schedule, plus the impulse's
// own centre: latencySamples() = P + (taps - 1) / 2 for a symmetric impulse.
//
// Spectra may be computed off the audio thread (computeSpectra into a pre-allocated Spectra) and handed to a channel
// with setSpectra(ch, &s, fadeSamples): the swap is a pointer exchange, and during the fade both the previous and the
// new spectra are accumulated and the outputs crossfaded (twice the multiply-accumulate for fadeSamples). The caller
// keeps the Spectra alive until the fade is over and the next swap has completed. Sized in prepare(); process(),
// setSpectra() and computeSpectra() never allocate.
#include "core/fir/Fft.h"
#include <algorithm>
#include <vector>

namespace mm::fir
{
class PartitionedConvolver
{
public:
    struct Spectra
    {
        std::vector<double> re, im; // K x 2P
        int K = 0, taps = 0;
    };

    // P = maxBlock rounded up to a power of two, at least minPartition (itself rounded up to a power of two, min 64)
    void prepare(int maxTaps, int maxBlock, int channels, int minPartition = 64)
    {
        P = 64;
        while (P < std::max(maxBlock, minPartition))
            P <<= 1;
        maxK = std::max(1, (std::max(1, maxTaps) + P - 1) / P);
        fft.prepare(2 * P);
        const size_t spec = static_cast<size_t>(2 * P);
        allocateSpectra(own);
        workRe.assign(spec, 0.0);
        workIm.assign(spec, 0.0);
        accRe.assign(spec, 0.0);
        accIm.assign(spec, 0.0);
        accRe2.assign(spec, 0.0);
        accIm2.assign(spec, 0.0);
        chans.assign(static_cast<size_t>(std::max(1, channels)), Channel{});
        for (auto& c : chans)
        {
            c.inFifo.assign(static_cast<size_t>(P), 0.0);
            c.outFifo.assign(static_cast<size_t>(P), 0.0);
            c.outFifoPrev.assign(static_cast<size_t>(P), 0.0);
            c.prevBlock.assign(static_cast<size_t>(P), 0.0);
            c.histRe.assign(static_cast<size_t>(maxK) * spec, 0.0);
            c.histIm.assign(static_cast<size_t>(maxK) * spec, 0.0);
            c.current = &own;
            c.previous = nullptr;
        }
        reset();
    }

    void allocateSpectra(Spectra& s) const
    {
        s.re.assign(static_cast<size_t>(maxK) * static_cast<size_t>(2 * P), 0.0);
        s.im.assign(static_cast<size_t>(maxK) * static_cast<size_t>(2 * P), 0.0);
        s.K = 0;
        s.taps = 0;
    }

    // K transforms of 2P points into a Spectra sized by allocateSpectra(); n <= the prepared maxTaps. Not for the
    // audio thread unless the spike is acceptable (K FFTs); uses the convolver's work buffers, so not concurrent with
    // process() either: callers computing off-thread use their own convolver-sized scratch (computeSpectraInto).
    void computeSpectra(const double* h, int n, Spectra& s) const
    {
        std::vector<double> re(static_cast<size_t>(2 * P)), im(static_cast<size_t>(2 * P));
        computeSpectraInto(h, n, s, re.data(), im.data());
    }
    // allocation-free form: `re` / `im` are 2P-sample scratch buffers owned by the caller
    void computeSpectraInto(const double* h, int n, Spectra& s, double* re, double* im) const noexcept
    {
        n = std::clamp(n, 0, maxK * P);
        s.taps = n;
        s.K = std::max(1, (n + P - 1) / P);
        const int spec = 2 * P;
        for (int j = 0; j < s.K; ++j)
        {
            std::fill(re, re + spec, 0.0);
            std::fill(im, im + spec, 0.0);
            const int from = j * P, count = std::min(P, n - from);
            for (int i = 0; i < count; ++i)
                re[i] = h[from + i];
            fft.forward(re, im);
            std::copy(re, re + spec, s.re.begin() + static_cast<std::ptrdiff_t>(j * spec));
            std::copy(im, im + spec, s.im.begin() + static_cast<std::ptrdiff_t>(j * spec));
        }
    }

    // n <= the prepared maxTaps: every channel uses this impulse from the next block boundary (no fade)
    void setImpulse(const double* h, int n) noexcept
    {
        computeSpectraInto(h, n, own, workRe.data(), workIm.data());
        for (auto& c : chans)
        {
            c.current = &own;
            c.previous = nullptr;
            c.fadeRemaining = 0;
        }
    }

    // hand channel `ch` new spectra; with fadeSamples > 0 the output crossfades from the previous spectra over that
    // many samples (rounded up to whole blocks of P). A swap during a fade finishes the fade from the current mix.
    void setSpectra(int ch, const Spectra* s, int fadeSamples) noexcept
    {
        auto& c = chans[static_cast<size_t>(ch)];
        if (s == c.current)
            return;
        c.previous = fadeSamples > 0 ? c.current : nullptr;
        c.current = s;
        c.fadeRemaining = fadeSamples > 0 ? fadeSamples : 0;
        c.fadeTotal = std::max(1, fadeSamples);
        c.fadePos = 0;
    }
    const Spectra* currentSpectra(int ch) const noexcept { return chans[static_cast<size_t>(ch)].current; }

    void process(float* buf, int n, int ch) noexcept
    {
        auto& c = chans[static_cast<size_t>(ch)];
        for (int i = 0; i < n; ++i)
        {
            c.inFifo[static_cast<size_t>(c.pos)] = static_cast<double>(buf[i]);
            double out = c.outFifo[static_cast<size_t>(c.pos)];
            if (c.fading)
            {
                const double t = std::min(1.0, static_cast<double>(c.fadePos++) / static_cast<double>(c.fadeTotal));
                out = c.outFifoPrev[static_cast<size_t>(c.pos)] + (out - c.outFifoPrev[static_cast<size_t>(c.pos)]) * t;
                if (c.fadePos >= c.fadeTotal)
                    c.fading = false; // the rest of this block's outFifo already holds the new output alone
            }
            buf[i] = static_cast<float>(out);
            if (++c.pos == P)
            {
                runBlock(c);
                c.pos = 0;
            }
        }
    }

    int latencySamples() const noexcept
    {
        const Spectra* s = chans.empty() ? &own : chans[0].current;
        return P + (std::max(s != nullptr ? s->taps : 0, 1) - 1) / 2;
    }
    int partitionSize() const noexcept { return P; }
    int numTaps() const noexcept { return chans.empty() ? own.taps : (chans[0].current != nullptr ? chans[0].current->taps : 0); }

    void reset() noexcept
    {
        for (auto& c : chans)
        {
            std::fill(c.inFifo.begin(), c.inFifo.end(), 0.0);
            std::fill(c.outFifo.begin(), c.outFifo.end(), 0.0);
            std::fill(c.outFifoPrev.begin(), c.outFifoPrev.end(), 0.0);
            std::fill(c.prevBlock.begin(), c.prevBlock.end(), 0.0);
            std::fill(c.histRe.begin(), c.histRe.end(), 0.0);
            std::fill(c.histIm.begin(), c.histIm.end(), 0.0);
            c.pos = 0;
            c.head = 0;
            c.previous = nullptr;
            c.fadeRemaining = 0;
            c.fading = false;
        }
    }

private:
    struct Channel
    {
        std::vector<double> inFifo, outFifo, outFifoPrev, prevBlock, histRe, histIm;
        int pos = 0, head = 0; // head: the ring slot holding the newest input spectrum
        const Spectra* current = nullptr;
        const Spectra* previous = nullptr; // during a fade: the spectra being faded out
        int fadeRemaining = 0, fadeTotal = 1, fadePos = 0;
        bool fading = false;
    };

    void accumulate(const Channel& c, const Spectra& s, double* ar, double* ai) const noexcept
    {
        const int spec = 2 * P;
        std::fill(ar, ar + spec, 0.0);
        std::fill(ai, ai + spec, 0.0);
        for (int j = 0; j < s.K; ++j)
        {
            int slot = c.head + j;
            if (slot >= maxK)
                slot -= maxK;
            const double* xr = c.histRe.data() + static_cast<std::ptrdiff_t>(slot * spec);
            const double* xi = c.histIm.data() + static_cast<std::ptrdiff_t>(slot * spec);
            const double* hr = s.re.data() + static_cast<std::ptrdiff_t>(j * spec);
            const double* hi = s.im.data() + static_cast<std::ptrdiff_t>(j * spec);
            for (int k = 0; k < spec; ++k)
            {
                ar[k] += xr[k] * hr[k] - xi[k] * hi[k];
                ai[k] += xr[k] * hi[k] + xi[k] * hr[k];
            }
        }
    }

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
        if (c.current != nullptr)
        {
            accumulate(c, *c.current, accRe.data(), accIm.data());
            fft.inverse(accRe.data(), accIm.data());
            std::copy(accRe.begin() + P, accRe.end(), c.outFifo.begin()); // the last P samples are free of wrap-around
        }
        else
            std::fill(c.outFifo.begin(), c.outFifo.end(), 0.0);
        if (c.previous != nullptr)
        {
            // a fade is pending or running: the block also gets the outgoing spectra's output
            accumulate(c, *c.previous, accRe2.data(), accIm2.data());
            fft.inverse(accRe2.data(), accIm2.data());
            std::copy(accRe2.begin() + P, accRe2.end(), c.outFifoPrev.begin());
            if (!c.fading)
            {
                c.fading = true;
                c.fadePos = 0;
            }
            c.fadeRemaining -= P;
            if (c.fadeRemaining <= 0)
                c.previous = nullptr; // last faded block: the fade position reaches 1 within it
        }
    }

    int P = 64, maxK = 1;
    Fft fft;
    Spectra own;
    std::vector<double> workRe, workIm, accRe, accIm, accRe2, accIm2;
    std::vector<Channel> chans;
};
} // namespace mm::fir
