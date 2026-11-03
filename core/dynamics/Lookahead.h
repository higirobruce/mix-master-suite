#pragma once
#include "core/delay/IntegerDelay.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace mm
{
// Lookahead: delays the audio by N samples and provides the running maximum of the detector level over the last
// N+1 samples, so the gain can be computed from the signal N samples ahead of the audio it is applied to. The sliding
// max is a block decomposition (van Herk / Gil-Werman): levels are grouped in blocks of N+1, and the max of any N+1
// consecutive levels is max(prefix max of the current block, suffix max of the previous block from the matching
// offset). Two comparisons per sample and no data-dependent branch (a monotonic deque cost 16 ns per sample on noise,
// this 3). Sized in prepare(); no allocation afterwards.
template <typename T> class Lookahead
{
public:
    void prepare(double sampleRate, double maxMs, int channels, int maxBlock = 1)
    {
        sr = sampleRate;
        maxSamples = std::max(1, static_cast<int>(std::lround(maxMs * 1e-3 * sr)));
        delay.prepare(maxSamples, channels, maxBlock);
        levels.assign(static_cast<size_t>(maxSamples + 1), kLowest);
        suffix.assign(static_cast<size_t>(maxSamples + 2), kLowest);
        setMs(currentMs);
        reset();
    }
    void setMs(double ms) noexcept
    {
        currentMs = std::clamp(ms, 0.0, 1000.0 * maxSamples / sr);
        const int w = std::clamp(static_cast<int>(std::lround(currentMs * 1e-3 * sr)), 0, maxSamples);
        if (w != window)
            restartHistory(w); // the sliding-max history restarts (a zero window keeps none)
        window = w;
        delay.setDelay(window);
    }
    int latencySamples() const noexcept { return window; }

    T delaySignal(T x, int ch) noexcept { return delay.process(x, ch); }
    void writeSignal(T x, int ch) noexcept { delay.write(x, ch); }
    T readDelayed(int samples, int ch) const noexcept { return delay.read(samples, ch); }
    // block forms (n <= the prepared maxBlock): write a block, then read it delayed by `samples`
    void writeSignalBlock(const T* in, int n, int ch) noexcept { delay.writeBlock(in, n, ch); }
    void readDelayedBlock(T* out, int n, int samples, int ch) const noexcept { delay.readBlock(out, n, samples, ch); }
    void setSamples(int samples) noexcept { setMs(1000.0 * samples / sr); }
    int maxWindowSamples() const noexcept { return maxSamples; }

    // Push the current detector level; returns max over the last window+1 levels (this one included).
    T maxEnvelope(T level) noexcept
    {
        if (window == 0)
            return level; // no lookahead: the maximum over one sample is the sample
        const int n = window + 1;
        levels[static_cast<size_t>(pos)] = level;
        prefix = pos == 0 ? level : std::max(prefix, level);
        const T out = std::max(prefix, suffix[static_cast<size_t>(pos + 1)]); // suffix[n] is the floor
        if (++pos == n)
        {
            // the block is complete: its suffix maxima serve the next block (n comparisons every n samples)
            T m = kLowest;
            for (int i = n - 1; i >= 0; --i)
            {
                m = std::max(m, levels[static_cast<size_t>(i)]);
                suffix[static_cast<size_t>(i)] = m;
            }
            pos = 0;
        }
        return out;
    }

    void reset() noexcept
    {
        delay.reset();
        restartHistory(window);
    }

private:
    void restartHistory(int w) noexcept
    {
        pos = 0;
        prefix = kLowest;
        std::fill(suffix.begin(), suffix.begin() + std::min<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(suffix.size()), w + 2), kLowest);
    }
    static constexpr T kLowest = std::numeric_limits<T>::lowest();
    IntegerDelay<T> delay;
    std::vector<T> levels, suffix; // the current block's levels; the previous block's suffix maxima (+ a floor entry)
    double sr = 48000.0, currentMs = 0.0;
    int maxSamples = 1, window = 0, pos = 0;
    T prefix = kLowest;
};
} // namespace mm
