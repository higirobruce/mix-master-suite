#include "core/metering/LoudnessMeter.h"
#include "core/metering/TruePeakMeter.h"
#include "helpers/Matrix.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <vector>

namespace
{
// stereo sine at `dbfs` per channel, fed in blocks of `block`
void feedSine(mm::LoudnessMeter& m, double sr, double hz, double dbfs, double seconds, int block = 480, double phase = 0.0)
{
    const double amp = std::pow(10.0, dbfs / 20.0);
    const int total = static_cast<int>(seconds * sr);
    std::vector<float> l(static_cast<size_t>(block)), r(static_cast<size_t>(block));
    const float* chans[2] = {l.data(), r.data()};
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min(block, total - pos);
        for (int i = 0; i < n; ++i)
            l[static_cast<size_t>(i)] = r[static_cast<size_t>(i)] = static_cast<float>(amp * std::sin(2.0 * std::numbers::pi * hz * (pos + i) / sr + phase));
        m.process(chans, n);
    }
}
} // namespace

TEST_CASE("K-weighting at 48 k matches BS.1770-4 Tables 1 and 2")
{
    const auto pre = mm::LoudnessMeter::preFilter(48000.0);
    REQUIRE(pre.b0 == Catch::Approx(1.53512485958697).margin(1e-5));
    REQUIRE(pre.b1 == Catch::Approx(-2.69169618940638).margin(1e-5));
    REQUIRE(pre.b2 == Catch::Approx(1.19839281085285).margin(1e-5));
    REQUIRE(pre.a1 == Catch::Approx(-1.69065929318241).margin(1e-5));
    REQUIRE(pre.a2 == Catch::Approx(0.73248077421585).margin(1e-5));
    const auto rlb = mm::LoudnessMeter::rlbFilter(48000.0);
    REQUIRE(rlb.b0 == Catch::Approx(1.0).margin(1e-5));
    REQUIRE(rlb.b1 == Catch::Approx(-2.0).margin(1e-5));
    REQUIRE(rlb.b2 == Catch::Approx(1.0).margin(1e-5));
    REQUIRE(rlb.a1 == Catch::Approx(-1.99004745483398).margin(1e-5));
    REQUIRE(rlb.a2 == Catch::Approx(0.99007225036621).margin(1e-5));
}

TEST_CASE("momentary and short-term loudness of a -23 dBFS stereo 1 kHz sine read -23.0 LUFS at four rates")
{
    for (double sr : mm::test::kMatrixRates)
    {
        mm::LoudnessMeter m;
        m.prepare(sr, 2, 60.0);
        feedSine(m, sr, 1000.0, -23.0, 4.0);
        INFO("sr " << sr);
        REQUIRE(m.momentaryLufs() == Catch::Approx(-23.0).margin(0.05));
        REQUIRE(m.shortTermLufs() == Catch::Approx(-23.0).margin(0.05));
        REQUIRE(m.integratedLufs() == Catch::Approx(-23.0).margin(0.05));
    }
}

TEST_CASE("integrated loudness gates out a quiet tail and reads the programme level")
{
    // EBU Tech 3341 seq-3341-3 shape: -36 dBFS for 10 s, -23 for 60 s, -36 for 10 s -> -23.0 +/- 0.1 LUFS
    mm::LoudnessMeter m;
    m.prepare(48000.0, 2, 120.0);
    feedSine(m, 48000.0, 1000.0, -36.0, 10.0);
    feedSine(m, 48000.0, 1000.0, -23.0, 60.0);
    feedSine(m, 48000.0, 1000.0, -36.0, 10.0);
    REQUIRE(m.integratedLufs() == Catch::Approx(-23.0).margin(0.1));
    // a tail 57 dB down is below the absolute gate and cannot drag the value
    feedSine(m, 48000.0, 1000.0, -80.0, 30.0);
    REQUIRE(m.integratedLufs() == Catch::Approx(-23.0).margin(0.1));
    REQUIRE(m.maxMomentaryLufs() == Catch::Approx(-23.0).margin(0.1));
    REQUIRE(m.maxShortTermLufs() == Catch::Approx(-23.0).margin(0.1));
}

TEST_CASE("loudness range of a two-level sequence (EBU Tech 3342 seq-1: -20 / -30 dBFS, 20 s each) is 10 LU")
{
    mm::LoudnessMeter m;
    m.prepare(48000.0, 2, 60.0);
    feedSine(m, 48000.0, 1000.0, -20.0, 20.0);
    feedSine(m, 48000.0, 1000.0, -30.0, 20.0);
    REQUIRE(m.loudnessRangeLu() == Catch::Approx(10.0).margin(1.0));
}

TEST_CASE("silence reads the floor and reset clears the histories")
{
    mm::LoudnessMeter m;
    m.prepare(48000.0, 2, 10.0);
    REQUIRE(m.integratedLufs() <= -100.0f);
    feedSine(m, 48000.0, 1000.0, -23.0, 2.0);
    m.reset();
    REQUIRE(m.integratedLufs() <= -100.0f);
    REQUIRE(m.momentaryLufs() <= -100.0f);
}

TEST_CASE("true-peak meter reads an inter-sample peak within 0.1 dBTP at four rates")
{
    // a sine at fs / 4 with phase pi / 4 has every sample at 0.707 of its amplitude: sample peak -3 dB below the true peak
    for (double sr : mm::test::kMatrixRates)
    {
        mm::TruePeakMeter tp;
        tp.prepare(sr, 2, 512);
        const double amp = 0.5; // -6.02 dBTP
        std::vector<float> l(512), r(512);
        const float* chans[2] = {l.data(), r.data()};
        float blockMax = -200.0f, sampleMax = 0.0f;
        for (int pos = 0; pos < static_cast<int>(sr); pos += 512)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float v = static_cast<float>(amp * std::sin(2.0 * std::numbers::pi * 0.25 * (pos + i) + std::numbers::pi / 4.0));
                l[static_cast<size_t>(i)] = v;
                r[static_cast<size_t>(i)] = -v;
                sampleMax = std::max(sampleMax, std::abs(v));
            }
            blockMax = std::max(blockMax, tp.processBlock(chans, 512));
        }
        INFO("sr " << sr);
        REQUIRE(20.0 * std::log10(sampleMax) == Catch::Approx(-9.03).margin(0.05));
        REQUIRE(tp.maxDbtp() == Catch::Approx(-6.02).margin(0.1));
        REQUIRE(blockMax == tp.maxDbtp());
        // and a plain full-band tone reads its amplitude
        mm::TruePeakMeter tp2;
        tp2.prepare(sr, 1, 512);
        const float* one[1] = {l.data()};
        for (int pos = 0; pos < static_cast<int>(sr); pos += 512)
        {
            for (int i = 0; i < 512; ++i)
                l[static_cast<size_t>(i)] = static_cast<float>(std::pow(10.0, -1.0 / 20.0) * std::sin(2.0 * std::numbers::pi * 1000.0 * (pos + i) / sr));
            tp2.processBlock(one, 512);
        }
        REQUIRE(tp2.maxDbtp() == Catch::Approx(-1.0).margin(0.05));
    }
}
