#pragma once
#include <array>
#include <atomic>

namespace mm
{
// Single-producer / single-consumer lock-free ring. Capacity N items. Audio thread pushes, GUI pops.
template <typename T, int N> class MeterFifo
{
public:
    bool push(const T& v) noexcept
    {
        const int w = writeIdx.load(std::memory_order_relaxed);
        const int next = (w + 1) % kSize;
        if (next == readIdx.load(std::memory_order_acquire))
            return false;
        slots[static_cast<size_t>(w)] = v;
        writeIdx.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept
    {
        const int r = readIdx.load(std::memory_order_relaxed);
        if (r == writeIdx.load(std::memory_order_acquire))
            return false;
        out = slots[static_cast<size_t>(r)];
        readIdx.store((r + 1) % kSize, std::memory_order_release);
        return true;
    }

    int available() const noexcept
    {
        const int w = writeIdx.load(std::memory_order_acquire);
        const int r = readIdx.load(std::memory_order_acquire);
        return (w - r + kSize) % kSize;
    }

private:
    static constexpr int kSize = N + 1;
    std::array<T, static_cast<size_t>(kSize)> slots{};
    std::atomic<int> writeIdx{0};
    std::atomic<int> readIdx{0};
};
} // namespace mm
