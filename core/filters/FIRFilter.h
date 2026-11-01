#pragma once
#include <algorithm>
#include <cstring>
#include <vector>

namespace mm
{
// Direct-form FIR with a power-of-two circular history per channel. Sized in prepare(); setTaps() never allocates
// and clamps to the prepared maximum. Linear-phase designs report (n-1)/2 samples of latency.
template <typename T> class FIRFilter
{
public:
    void prepare(int maxTaps, int channels)
    {
        maxTapCount = std::max(1, maxTaps);
        size = 1;
        while (size < maxTapCount)
            size <<= 1;
        mask = size - 1;
        numChannels = std::max(1, channels);
        history.assign(static_cast<size_t>(size) * static_cast<size_t>(numChannels), T(0));
        writePos.assign(static_cast<size_t>(numChannels), 0);
        taps.assign(static_cast<size_t>(maxTapCount), T(0));
        tapCount = 0;
    }

    template <typename U> void setTaps(const U* newTaps, int n) noexcept
    {
        tapCount = std::clamp(n, 0, static_cast<int>(taps.size())); // 0 until prepare(): safe to call early
        for (int i = 0; i < tapCount; ++i)
            taps[static_cast<size_t>(i)] = static_cast<T>(newTaps[i]);
    }

    int numTaps() const noexcept { return tapCount; }
    int latencySamples() const noexcept { return tapCount > 0 ? (tapCount - 1) / 2 : 0; }

    T process(T x, int ch) noexcept
    {
        T* h = history.data() + static_cast<size_t>(ch) * static_cast<size_t>(size);
        int& w = writePos[static_cast<size_t>(ch)];
        h[w] = x;
        T acc = T(0);
        int idx = w;
        for (int k = 0; k < tapCount; ++k)
        {
            acc += taps[static_cast<size_t>(k)] * h[idx];
            idx = (idx - 1) & mask;
        }
        w = (w + 1) & mask;
        return acc;
    }
    void process(T* buf, int n, int ch) noexcept
    {
        for (int i = 0; i < n; ++i)
            buf[i] = process(buf[i], ch);
    }
    void reset() noexcept
    {
        std::fill(history.begin(), history.end(), T(0));
        std::fill(writePos.begin(), writePos.end(), 0);
    }

private:
    std::vector<T> history, taps;
    std::vector<int> writePos;
    int size = 1, mask = 0, maxTapCount = 1, tapCount = 0, numChannels = 1;
};
} // namespace mm
