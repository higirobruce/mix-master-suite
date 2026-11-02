#include "core/reverb/DattorroPlate.h"
#include "core/reverb/EarlyReflections.h"
#include "core/reverb/FDN.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numeric>

TEST_CASE("Hadamard mixing preserves energy and FDN line lengths are mutually prime after scaling")
{
    std::array<float, 16> v{};
    for (int i = 0; i < 16; ++i)
        v[static_cast<size_t>(i)] = std::sin(0.7f * static_cast<float>(i) + 0.3f);
    const auto in = v;
    mm::reverb::hadamard16(v.data());
    double eIn = 0.0, eOut = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        eIn += in[static_cast<size_t>(i)] * in[static_cast<size_t>(i)];
        eOut += v[static_cast<size_t>(i)] * v[static_cast<size_t>(i)];
    }
    REQUIRE(eOut == Catch::Approx(eIn).epsilon(1e-5));

    mm::reverb::FDN<float> fdn;
    fdn.prepare(48000.0, 0.3);
    for (double size : {0.1, 0.6, 1.0})
    {
        fdn.setAlgorithm(mm::reverb::FdnAlgorithm::Hall, size);
        const auto lengths = fdn.lengthsSamples();
        for (size_t a = 0; a < lengths.size(); ++a)
            for (size_t b = a + 1; b < lengths.size(); ++b)
                REQUIRE(std::gcd(lengths[a], lengths[b]) == 1);
        REQUIRE(lengths[0] >= 48); // never shorter than 1 ms
    }
}

TEST_CASE("FDN and plate: an impulse decays, stays finite and lands near silence after 3 x RT60")
{
    mm::reverb::FDN<float> fdn;
    fdn.prepare(48000.0, 0.3);
    fdn.setAlgorithm(mm::reverb::FdnAlgorithm::Room, 0.6);
    fdn.setRT60(0.5);
    fdn.setDamping(0.0f);
    fdn.setDiffusion(0.7f);
    float peak = 0.0f, late = 0.0f;
    for (int n = 0; n < 48000 * 2; ++n)
    {
        float l = 0.0f, r = 0.0f;
        fdn.processStereo(n == 0 ? 1.0f : 0.0f, n == 0 ? 1.0f : 0.0f, l, r);
        REQUIRE(std::isfinite(l));
        peak = std::max(peak, std::abs(l));
        if (n > 48000 * 3 / 2)
            late = std::max(late, std::abs(l));
    }
    REQUIRE(peak > 0.01f);
    REQUIRE(late < peak * 1e-3f); // > 60 dB down after 3 x RT60

    mm::reverb::DattorroPlate<float> plate;
    plate.prepare(48000.0);
    plate.setRT60(0.5);
    plate.setDamping(0.0f);
    peak = late = 0.0f;
    for (int n = 0; n < 48000 * 2; ++n)
    {
        float l = 0.0f, r = 0.0f;
        plate.processStereo(n == 0 ? 1.0f : 0.0f, n == 0 ? 1.0f : 0.0f, l, r);
        REQUIRE(std::isfinite(l));
        peak = std::max(peak, std::abs(l));
        if (n > 48000 * 3 / 2)
            late = std::max(late, std::abs(l));
    }
    REQUIRE(peak > 0.01f);
    REQUIRE(late < peak * 1e-3f);

    mm::reverb::EarlyReflections<float> er;
    er.prepare(48000.0);
    er.setPattern(mm::reverb::ErPattern::Hall, 1.0);
    int taps = 0;
    for (int n = 0; n < 48000 / 4; ++n)
    {
        float l = 0.0f, r = 0.0f;
        er.processStereo(n == 0 ? 1.0f : 0.0f, n == 0 ? 1.0f : 0.0f, l, r);
        if (std::abs(l) > 1e-3f || std::abs(r) > 1e-3f)
            ++taps;
    }
    REQUIRE(taps == 8);
}
