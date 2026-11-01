#include "core/metering/Correlation.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

namespace
{
std::vector<float> noise(unsigned seed, size_t n)
{
    std::vector<float> v(n);
    for (auto& x : v)
    {
        seed = seed * 1664525u + 1013904223u;
        x = (static_cast<float>(seed >> 8) / 8388608.0f) - 1.0f;
    }
    return v;
}
} // namespace

TEST_CASE("correlation meter: identical +1, inverted -1, independent noise near 0")
{
    const double sr = 48000.0;
    auto a = noise(1, 48000), b = noise(2, 48000);
    std::vector<float> inv(a.size());
    for (size_t i = 0; i < a.size(); ++i)
        inv[i] = -a[i];
    mm::CorrelationMeter m;
    m.prepare(sr, 300.0);
    for (size_t i = 0; i + 64 <= a.size(); i += 64)
        m.process(a.data() + i, a.data() + i, 64);
    REQUIRE(m.value() == Catch::Approx(1.0f).margin(0.001));
    m.reset();
    for (size_t i = 0; i + 64 <= a.size(); i += 64)
        m.process(a.data() + i, inv.data() + i, 64);
    REQUIRE(m.value() == Catch::Approx(-1.0f).margin(0.001));
    m.reset();
    for (size_t i = 0; i + 64 <= a.size(); i += 64)
        m.process(a.data() + i, b.data() + i, 64);
    REQUIRE(std::abs(m.value()) < 0.05f);
    m.reset();
    std::vector<float> zero(64, 0.0f);
    m.process(zero.data(), zero.data(), 64);
    REQUIRE(std::isfinite(m.value()));
}
