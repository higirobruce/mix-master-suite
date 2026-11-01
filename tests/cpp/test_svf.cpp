#include "core/filters/SVF.h"
#include "helpers/Matrix.h"
#include "helpers/Response.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Mode = mm::SVF<double>::Mode;
static const Mode kModes[] = {Mode::LowPass, Mode::HighPass, Mode::BandPass, Mode::Notch,
                              Mode::Peak,    Mode::LowShelf, Mode::HighShelf, Mode::AllPass};

TEST_CASE("SVF implementation matches its analytic transfer function")
{
    for (double sr : mm::test::kMatrixRates)
    {
        const auto band = mm::test::measureBand(sr);
        const auto freqs = mm::test::logFrequencies(band.lowHz, band.highHz, 40);
        for (Mode m : kModes)
            for (double f : {100.0, 1000.0, 8000.0})
                for (double q : {0.5, 2.0})
                {
                    mm::SVF<double> svf;
                    svf.prepare(sr);
                    svf.setMode(m);
                    svf.setParams(f, q, 6.0);
                    const auto ir = mm::test::impulseResponse([&](double x) { return svf.process(x); }, 65536);
                    for (double fm : freqs)
                    {
                        if (m == Mode::Notch && std::abs(fm - f) / f < 0.05)
                            continue;
                        const auto meas = mm::test::responseAt(ir, sr, fm);
                        const auto exp = svf.evaluate(fm);
                        INFO("mode " << static_cast<int>(m) << " sr " << sr << " f0 " << f << " q " << q << " at " << fm);
                        if (std::abs(exp) < 1e-5) // deep stopband: below float relevance
                            continue;
                        REQUIRE(std::abs(mm::test::magDb(meas) - mm::test::magDb(exp)) < 0.1);
                        REQUIRE(std::abs(mm::test::phaseDiffDeg(mm::test::phaseDeg(meas), mm::test::phaseDeg(exp))) < 2.0);
                    }
                }
    }
}

TEST_CASE("SVF unity modes are transparent")
{
    mm::SVF<float> peak, ap;
    peak.prepare(48000.0);
    peak.setMode(Mode::Peak);
    peak.setParams(1000.0, 1.0, 0.0);
    ap.prepare(48000.0);
    ap.setMode(Mode::AllPass);
    ap.setParams(1000.0, 1.0, 0.0);
    float maxDiff = 0.0f;
    double energyIn = 0.0, energyOut = 0.0;
    for (int n = 0; n < 48000; ++n)
    {
        const float x = 0.5f * std::sin(0.07f * static_cast<float>(n));
        maxDiff = std::max(maxDiff, std::abs(peak.process(x) - x));
        const float y = ap.process(x);
        energyIn += x * x;
        energyOut += y * y;
    }
    REQUIRE(20.0 * std::log10(static_cast<double>(maxDiff) + 1e-30) < -120.0);
    REQUIRE(10.0 * std::log10(energyOut / energyIn) == Catch::Approx(0.0).margin(0.02));
}

TEST_CASE("SVF stays stable under per-sample cutoff modulation at high Q")
{
    const double sr = 48000.0;
    mm::SVF<float> svf;
    svf.prepare(sr);
    svf.setMode(Mode::LowPass);
    float peak = 0.0f;
    unsigned seed = 1;
    for (int n = 0; n < 480000; ++n)
    {
        const double lfo = 0.5 * (1.0 + std::sin(2.0 * 3.14159265 * 5.0 * n / sr));
        svf.setParams(20.0 * std::pow(900.0, lfo), 10.0, 0.0);
        seed = seed * 1664525u + 1013904223u;
        const float noise = (static_cast<float>(seed >> 8) / 8388608.0f) - 1.0f;
        const float y = svf.process(0.25f * noise);
        REQUIRE(std::isfinite(y));
        peak = std::max(peak, std::abs(y));
    }
    REQUIRE(peak < 31.6f); // < +30 dB
}

TEST_CASE("SVF edge parameters are finite")
{
    for (double sr : mm::test::kMatrixRates)
        for (Mode m : kModes)
            for (double f : {20.0, 0.49 * sr})
                for (double q : {0.1, 10.0})
                {
                    mm::SVF<float> svf;
                    svf.prepare(sr);
                    svf.setMode(m);
                    svf.setParams(f, q, 12.0);
                    for (int n = 0; n < 20000; ++n)
                        REQUIRE(std::isfinite(svf.process(n == 0 ? 1.0f : 0.0f)));
                }
}
