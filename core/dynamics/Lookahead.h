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
        const int w = std::clamp(static_cast<int>(std::lround(currentMs * 1e-3 * sr)), 0, maxSamples);
        if (w != window)
            head = count = 0; // the sliding-max history restarts (a zero window keeps none)
        window = w;
        delay.setDelay(window);
    }
    int latencySamples() const noexcept { return window; }

    T delaySignal(T x, int ch) noexcept { return delay.process(x, ch); }
    void writeSignal(T x, int ch) noexcept { delay.write(x, ch); }
    T readDelayed(int samples, int ch) const noexcept { return delay.read(samples, ch); }
    void setSamples(int samples) noexcept { setMs(1000.0 * samples / sr); }
    int maxWindowSamples() const noexcept { return maxSamples; }

    // Push the current detector level; returns max over the last window+1 levels (this one included).
    // Monotonic deque in a ring; indices wrap by comparison (an integer modulo per sample costs ~25 cycles on x86).
    T maxEnvelope(T level) noexcept
    {
        if (window == 0)
            return level; // no lookahead: the maximum over one sample is the sample
        ++sampleIndex;
        // drop smaller values from the back (they can never be the max again)
        int back = head + count; // one past the last element
        if (back >= capacity)
            back -= capacity;
        while (count > 0)
        {
            const int last = back == 0 ? capacity - 1 : back - 1;
            if (dqValue[static_cast<size_t>(last)] > level)
                break;
            back = last;
            --count;
        }
        dqValue[static_cast<size_t>(back)] = level;
        dqIndex[static_cast<size_t>(back)] = sampleIndex;
        ++count;
        // drop the front if it left the window
        const long long oldest = sampleIndex - (window + 1);
        while (count > 0 && dqIndex[static_cast<size_t>(head)] <= oldest)
        {
            if (++head == capacity)
                head = 0;
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
