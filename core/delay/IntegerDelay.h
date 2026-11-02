#pragma once
#include <algorithm>
#include <vector>

namespace mm
{
// Integer-sample delay, per channel. Sized in prepare(); process() never allocates.
template <typename T> class IntegerDelay
{
public:
    void prepare(int maxSamples, int channels)
    {
        maxDelay = std::max(0, maxSamples);
        length = maxDelay + 1;
        buffer.assign(static_cast<size_t>(length) * static_cast<size_t>(std::max(1, channels)), T(0));
        writePos.assign(static_cast<size_t>(std::max(1, channels)), 0);
        numChannels = std::max(1, channels);
        setDelay(delay);
    }

    void setDelay(int samples) noexcept { delay = std::clamp(samples, 0, maxDelay); }
    int getDelay() const noexcept { return delay; }

    T process(T in, int ch) noexcept
    {
        write(in, ch);
        return read(delay, ch);
    }

    // write then read at any delay (0 = the sample just written) so a changing delay can crossfade two taps
    void write(T in, int ch) noexcept
    {
        T* buf = buffer.data() + static_cast<size_t>(ch) * static_cast<size_t>(length);
        int& w = writePos[static_cast<size_t>(ch)];
        buf[w] = in;
        if (++w == length)
            w = 0;
    }
    T read(int delaySamples, int ch) const noexcept
    {
        const T* buf = buffer.data() + static_cast<size_t>(ch) * static_cast<size_t>(length);
        int r = writePos[static_cast<size_t>(ch)] - 1 - std::clamp(delaySamples, 0, maxDelay);
        if (r < 0)
            r += length;
        return buf[r];
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), T(0));
        std::fill(writePos.begin(), writePos.end(), 0);
    }

private:
    std::vector<T> buffer;
    std::vector<int> writePos;
    int length = 1;
    int maxDelay = 0;
    int delay = 0;
    int numChannels = 1;
};
} // namespace mm
