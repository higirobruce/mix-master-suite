#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace mm
{
enum class DelayInterp
{
    Lagrange3, // 4-point Lagrange: exact at integer delays, < 0.1 dB droop below 0.1 fs at d = 0.5
    Thiran1    // first-order all-pass: flat magnitude, fractional phase accurate at low frequencies
};

// Fractional delay with a power-of-two ring per channel. Delay-time changes slew linearly over setSlewMs() so a
// jump produces a pitch glide instead of a click. Sized in prepare(); nothing allocates afterwards.
template <typename T> class FractionalDelayLine
{
public:
    using Interp = DelayInterp;
    void prepare(double sampleRate, double maxSeconds, int channels)
    {
        sr = sampleRate;
        maxDelay = std::max(4.0, maxSeconds * sr);
        size = 1;
        while (size < static_cast<int>(maxDelay) + 8)
            size <<= 1;
        mask = size - 1;
        numChannels = std::max(1, channels);
        buffer.assign(static_cast<size_t>(size) * static_cast<size_t>(numChannels), T(0));
        state.assign(static_cast<size_t>(numChannels), {});
        setSlewMs(slewMs);
        reset();
    }
    void setInterp(DelayInterp i) noexcept { interp = i; }
    void setSlewMs(double ms) noexcept
    {
        slewMs = std::max(ms, 0.0);
        slewSamples = std::max(1.0, slewMs * 1e-3 * sr);
    }
    void setDelaySamples(float d, int ch) noexcept
    {
        auto& s = state[static_cast<size_t>(ch)];
        s.target = clampDelay(static_cast<double>(d));
        if (slewMs <= 0.0)
            s.current = s.target;
        else
            s.increment = (s.target - s.current) / slewSamples;
    }
    double currentDelay(int ch) const noexcept { return state[static_cast<size_t>(ch)].current; }

    void push(T x, int ch) noexcept
    {
        auto& s = state[static_cast<size_t>(ch)];
        buffer[static_cast<size_t>(ch) * static_cast<size_t>(size) + static_cast<size_t>(s.write)] = x;
        s.write = (s.write + 1) & mask;
    }

    // Read at the (slewed) current delay; call once per push.
    T read(int ch) noexcept
    {
        auto& s = state[static_cast<size_t>(ch)];
        if (s.current != s.target)
        {
            s.current += s.increment;
            if ((s.increment > 0.0 && s.current >= s.target) || (s.increment < 0.0 && s.current <= s.target))
                s.current = s.target;
        }
        return readAt(static_cast<float>(s.current), ch);
    }

    // Read at an arbitrary delay (samples) relative to the last pushed sample (delay 0 = that sample).
    T readAt(float delaySamples, int ch) noexcept
    {
        auto& s = state[static_cast<size_t>(ch)];
        const T* b = buffer.data() + static_cast<size_t>(ch) * static_cast<size_t>(size);
        const double d = clampDelay(static_cast<double>(delaySamples));
        auto at = [&](int delayInt) { return b[(s.write - 1 - delayInt) & mask]; };
        if (interp == DelayInterp::Lagrange3)
        {
            const int n = static_cast<int>(d);
            const double f = d - n;
            const double h0 = -f * (f - 1.0) * (f - 2.0) / 6.0;
            const double h1 = (f + 1.0) * (f - 1.0) * (f - 2.0) / 2.0;
            const double h2 = -(f + 1.0) * f * (f - 2.0) / 2.0;
            const double h3 = (f + 1.0) * f * (f - 1.0) / 6.0;
            return static_cast<T>(h0 * at(n - 1) + h1 * at(n) + h2 * at(n + 1) + h3 * at(n + 2));
        }
        // Thiran: integer part N, fractional d1 in [0.5, 1.5) realised by a first-order all-pass
        int N = static_cast<int>(std::floor(d - 0.5));
        if (N < 0)
            N = 0;
        const double d1 = d - N;
        const double a = (1.0 - d1) / (1.0 + d1);
        const double v = static_cast<double>(at(N));
        const double y = a * v + s.apX1 - a * s.apY1;
        s.apX1 = v;
        s.apY1 = y;
        return static_cast<T>(y);
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), T(0));
        for (auto& s : state)
        {
            s.write = 0;
            s.apX1 = s.apY1 = 0.0;
            s.current = s.target;
        }
    }

private:
    double clampDelay(double d) const noexcept
    {
        return std::clamp(d, interp == DelayInterp::Lagrange3 ? 1.0 : 0.5, maxDelay);
    }
    struct ChannelState
    {
        int write = 0;
        double current = 1.0, target = 1.0, increment = 0.0;
        double apX1 = 0.0, apY1 = 0.0;
    };
    std::vector<T> buffer;
    std::vector<ChannelState> state;
    DelayInterp interp = DelayInterp::Lagrange3;
    double sr = 48000.0, maxDelay = 4.0, slewMs = 20.0, slewSamples = 960.0;
    int size = 8, mask = 7, numChannels = 1;
};
} // namespace mm
