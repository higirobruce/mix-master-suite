#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/Transfer.h"
#include "helpers/Matrix.h"
#include "helpers/Response.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
enum Type { Peak, LowShelf, HighShelf, LowPass, HighPass, Notch, AllPass };

mm::BiquadCoeffs<double> design(Type t, double sr, double f, double gainDb, double q)
{
    switch (t)
    {
    case Peak: return mm::rbj::peak(sr, f, gainDb, q);
    case LowShelf: return mm::rbj::lowShelf(sr, f, gainDb, q);
    case HighShelf: return mm::rbj::highShelf(sr, f, gainDb, q);
    case LowPass: return mm::rbj::lowPass(sr, f, q);
    case HighPass: return mm::rbj::highPass(sr, f, q);
    case Notch: return mm::rbj::notch(sr, f, q);
    default: return mm::rbj::allPass(sr, f, q);
    }
}
} // namespace

TEST_CASE("biquad implementation matches its analytic transfer function within 0.1 dB / 2 deg")
{
    for (double sr : mm::test::kMatrixRates)
    {
        const auto band = mm::test::measureBand(sr);
        const auto freqs = mm::test::logFrequencies(band.lowHz, band.highHz, 60);
        for (Type t : {Peak, LowShelf, HighShelf, LowPass, HighPass, Notch, AllPass})
            for (double f : {50.0, 200.0, 1000.0, 5000.0, 15000.0})
            {
                if (f > 0.45 * sr)
                    continue;
                for (double gain : {-12.0, 6.0})
                    for (double q : {0.5, 2.0})
                    {
                        const auto c = design(t, sr, f, gain, q);
                        mm::Biquad<double> bq;
                        bq.setCoeffs(c);
                        const auto ir = mm::test::impulseResponse([&](double x) { return bq.process(x); }, 65536);
                        for (double fm : freqs)
                        {
                            // notches are singular at f0: skip the deep null itself (any measurement is noise there)
                            if (t == Notch && std::abs(fm - f) / f < 0.05)
                                continue;
                            const auto measured = mm::test::responseAt(ir, sr, fm);
                            const auto expected = mm::evaluate(c, sr, fm);
                            INFO("type " << t << " sr " << sr << " f0 " << f << " gain " << gain << " q " << q << " at " << fm);
                            REQUIRE(std::abs(mm::test::magDb(measured) - mm::test::magDb(expected)) < 0.1);
                            REQUIRE(std::abs(mm::test::phaseDiffDeg(mm::test::phaseDeg(measured), mm::test::phaseDeg(expected))) < 2.0);
                        }
                    }
            }
    }
}

TEST_CASE("unity peak filter nulls the input")
{
    mm::Biquad<float> bf;
    bf.setCoeffs(mm::BiquadCoeffs<float>::fromDouble(mm::rbj::peak(48000.0, 1000.0, 0.0, 1.0)));
    mm::Biquad<double> bd;
    bd.setCoeffs(mm::rbj::peak(48000.0, 1000.0, 0.0, 1.0));
    float maxF = 0.0f;
    double maxD = 0.0;
    for (int n = 0; n < 48000; ++n)
    {
        const float x = std::sin(0.1f * static_cast<float>(n)) * 0.5f;
        maxF = std::max(maxF, std::abs(bf.process(x) - x));
        maxD = std::max(maxD, std::abs(bd.process(static_cast<double>(x)) - static_cast<double>(x)));
    }
    REQUIRE(20.0 * std::log10(static_cast<double>(maxF) + 1e-30) < -120.0);
    REQUIRE(20.0 * std::log10(maxD + 1e-300) < -200.0);
}

TEST_CASE("biquad edge parameters stay finite and bounded")
{
    for (double sr : mm::test::kMatrixRates)
        for (Type t : {Peak, LowShelf, HighShelf, LowPass, HighPass, Notch, AllPass})
            for (double f : {20.0, 0.49 * sr})
                for (double q : {0.1, 10.0})
                {
                    const auto c = design(t, sr, f, 12.0, q);
                    mm::Biquad<float> bq;
                    bq.setCoeffs(mm::BiquadCoeffs<float>::fromDouble(c));
                    float peak = 0.0f;
                    for (int n = 0; n < 20000; ++n)
                    {
                        const float y = bq.process(n == 0 ? 1.0f : 0.0f);
                        REQUIRE(std::isfinite(y));
                        peak = std::max(peak, std::abs(y));
                    }
                    REQUIRE(peak < 20.0f); // < +26 dB impulse peak even at Q 10, +12 dB
                }
}

TEST_CASE("biquad stays stable while coefficients sweep every sample")
{
    const double sr = 48000.0;
    mm::Biquad<float> bq;
    float peak = 0.0f;
    for (int n = 0; n < 480000; ++n)
    {
        const double f = 20.0 * std::pow(1000.0, 0.5 * (1.0 + std::sin(2.0 * 3.14159265 * 0.5 * n / sr)));
        bq.setCoeffs(mm::BiquadCoeffs<float>::fromDouble(mm::rbj::peak(sr, f, 12.0, 4.0)));
        const float y = bq.process(std::sin(0.3f * static_cast<float>(n)));
        REQUIRE(std::isfinite(y));
        peak = std::max(peak, std::abs(y));
    }
    REQUIRE(peak < 16.0f); // < +24 dB
}

TEST_CASE("float biquad at 20 Hz / high Q / 192 kHz: double meets the spec, float deviation is recorded")
{
    const double sr = 192000.0;
    const auto c = mm::rbj::peak(sr, 20.0, 6.0, 10.0);
    mm::Biquad<double> bd;
    bd.setCoeffs(c);
    mm::Biquad<float> bf;
    bf.setCoeffs(mm::BiquadCoeffs<float>::fromDouble(c));
    const auto irD = mm::test::impulseResponse([&](double x) { return bd.process(x); }, 1 << 20);
    const auto irF = mm::test::impulseResponse([&](double x) { return static_cast<double>(bf.process(static_cast<float>(x))); }, 1 << 20);
    double worstD = 0.0, worstF = 0.0;
    for (double f : {15.0, 20.0, 25.0, 40.0, 100.0})
    {
        const auto e = mm::evaluate(c, sr, f);
        worstD = std::max(worstD, std::abs(mm::test::magDb(mm::test::responseAt(irD, sr, f)) - mm::test::magDb(e)));
        worstF = std::max(worstF, std::abs(mm::test::magDb(mm::test::responseAt(irF, sr, f)) - mm::test::magDb(e)));
    }
    INFO("double worst " << worstD << " dB, float worst " << worstF << " dB");
    REQUIRE(worstD < 0.1);
    REQUIRE(worstF < 6.0); // characterisation: float coefficients at 20 Hz / 192 k are not EQ-grade (ADR-0003: EQ runs double)
}
