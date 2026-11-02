#pragma once
#include "core/delay/IntegerDelay.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace mm
{
// Lookahead: delays the audio by N samples and provides the running maximum of the detector level over the last
// N+1 samples (a monotonic deque, O(1) amortised), so the gain can be computed from the signal N samples ahead of
// the audio it is applied to. Sized in prepare(); no allocation afterwards.
template <typename T> class Lookahead
{
public:
    void prepare(double sampleRate, double maxMs, int channels)
    {
        sr = sampleRate;
        maxSamples = std::max(1, static_cast<int>(std::lround(maxMs * 1e-3 * sr)));
        delay.prepare(maxSamples, channels);
        const int cap = maxSamples + 2;
        dqValue.assign(static_cast<size_t>(cap), T(0));
        dqIndex.assign(static_cast<size_t>(cap), 0);
        capacity = cap;
        setMs(currentMs);
        reset();
    }
    void setMs(double ms) noexcept
    {
        currentMs = std::clamp(ms, 0.0, 1000.0 * maxSamples / sr);
        window = std::clamp(static_cast<int>(std::lround(currentMs * 1e-3 * sr)), 0, maxSamples);
        delay.setDelay(window);
    }
    int latencySamples() const noexcept { return window; }

    T delaySignal(T x, int ch) noexcept { return delay.process(x, ch); }
    void writeSignal(T x, int ch) noexcept { delay.write(x, ch); }
    T readDelayed(int samples, int ch) const noexcept { return delay.read(samples, ch); }
    void setSamples(int samples) noexcept { setMs(1000.0 * samples / sr); }
    int maxWindowSamples() const noexcept { return maxSamples; }

    // Push the current detector level; returns max over the last window+1 levels (this one included).
    T maxEnvelope(T level) noexcept
    {
        ++sampleIndex;
        // drop smaller values from the back (they can never be the max again)
        while (count > 0 && dqValue[static_cast<size_t>((head + count - 1) % capacity)] <= level)
            --count;
        const int slot = (head + count) % capacity;
        dqValue[static_cast<size_t>(slot)] = level;
        dqIndex[static_cast<size_t>(slot)] = sampleIndex;
        ++count;
        // drop the front if it left the window
        while (count > 0 && dqIndex[static_cast<size_t>(head)] <= sampleIndex - (window + 1))
        {
            head = (head + 1) % capacity;
            --count;
        }
        return dqValue[static_cast<size_t>(head)];
    }

    void reset() noexcept
    {
        delay.reset();
        head = count = 0;
        sampleIndex = 0;
    }

private:
    IntegerDelay<T> delay;
    std::vector<T> dqValue;
    std::vector<long long> dqIndex;
    double sr = 48000.0, currentMs = 0.0;
    int maxSamples = 1, window = 0, capacity = 2, head = 0, count = 0;
    long long sampleIndex = 0;
};
} // namespace mm
