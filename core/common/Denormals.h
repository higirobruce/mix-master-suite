#pragma once
// Flush-to-zero / denormals-are-zero for the duration of a scope (audio thread).
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#include <pmmintrin.h>
#define MM_X86_FTZ 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define MM_ARM64_FZ 1
#endif
#include <cstdint>

namespace mm
{
struct ScopedNoDenormals
{
    ScopedNoDenormals() noexcept
    {
#if MM_X86_FTZ
        saved = _mm_getcsr();
        _mm_setcsr(saved | 0x8040); // FTZ | DAZ
#elif MM_ARM64_FZ
        std::uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        saved = fpcr;
        fpcr |= (1ull << 24); // FZ
        asm volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
    }
    ~ScopedNoDenormals() noexcept
    {
#if MM_X86_FTZ
        _mm_setcsr(saved);
#elif MM_ARM64_FZ
        asm volatile("msr fpcr, %0" : : "r"(saved));
#endif
    }
    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
#if MM_X86_FTZ
    unsigned int saved = 0;
#elif MM_ARM64_FZ
    std::uint64_t saved = 0;
#endif
};
} // namespace mm
