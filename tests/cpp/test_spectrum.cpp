#include "core/metering/Spectrum.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

TEST_CASE("spectrum analyser reads a 0 dBFS sine at the right bin and level")
{
    const double sr = 48000.0;
    mm::SpectrumAnalyser sa;
    sa.prepare(sr, 12, 1); // 4096
    const double f0 = 85.0 * sr / 4096.0; // bin-centred (996.1 Hz): no Hann scalloping loss on the level check
    auto x = mm::test::sine(f0, sr, 1.0);
    std::vector<float> mag;
    bool got = false;
    for (size_t i = 0; i + 64 <= x.size(); i += 64)
    {
        { const float* ch[1] = {x.data() + i}; sa.push(ch, 1, 64); }
        if (sa.nextFrame(mag))
            got = true;
    }
    REQUIRE(got);
    REQUIRE(mag.size() == 2049);
    size_t peak = 0;
    for (size_t k = 1; k < mag.size(); ++k)
        if (mag[k] > mag[peak])
            peak = k;
    const double binHz = sr / 4096.0;
    REQUIRE(std::abs(static_cast<double>(peak) * binHz - f0) <= binHz);
    REQUIRE(mag[peak] == Catch::Approx(0.0f).margin(0.5f));
    REQUIRE(mag[peak + 200] < -60.0f);
}

TEST_CASE("spectrum analyser push/nextFrame from two threads loses nothing")
{
    mm::SpectrumAnalyser sa;
    sa.prepare(48000.0, 12, 1);
    std::atomic<bool> done{false};
    std::atomic<int> frames{0};
    std::thread consumer([&] {
        std::vector<float> mag;
        while (!done.load())
            if (sa.nextFrame(mag))
                ++frames;
        std::vector<float> m2;
        while (sa.nextFrame(m2))
            ++frames;
    });
    std::vector<float> block(64, 0.1f);
    const int blocks = 4096 * 50 / 64; // 50 full windows
    for (int b = 0; b < blocks; ++b)
    {
        const float* ch[1] = {block.data()};
        while (!sa.push(ch, 1, 64))
            std::this_thread::yield();
    }
    done = true;
    consumer.join();
    REQUIRE(frames.load() >= 49); // 50 % overlap gives ~2 frames per window; at least one per window
}

TEST_CASE("spectrum analyser pushes all channels atomically")
{
    mm::SpectrumAnalyser sa;
    sa.prepare(48000.0, 10, 2); // 1024-point, capacity 4096 per channel
    std::vector<float> l(64, 0.5f), r(64, -0.5f);
    const float* chans[2] = {l.data(), r.data()};
    // fill the ring nearly full, then a push that cannot fit must be rejected for BOTH channels (nothing committed)
    int accepted = 0;
    for (int i = 0; i < 200; ++i)
        if (sa.push(chans, 2, 64))
            ++accepted;
    REQUIRE(accepted == 63); // (capacity - 1) / 64
    std::vector<float> mag;
    REQUIRE(sa.nextFrame(mag));
    // L + R average is zero for this signal: a desynchronised ring would leave non-zero bins
    for (float v : mag)
        REQUIRE(v < -100.0f);
}
