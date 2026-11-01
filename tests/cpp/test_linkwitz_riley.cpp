#include "core/filters/LinkwitzRiley.h"
#include "helpers/Matrix.h"
#include "helpers/Response.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Order = mm::LinkwitzRiley<double>::Order;

TEST_CASE("Linkwitz-Riley outputs match their analytic responses and sum to an all-pass")
{
    for (double sr : mm::test::kMatrixRates)
    {
        const auto band = mm::test::measureBand(sr);
        const auto freqs = mm::test::logFrequencies(band.lowHz, band.highHz, 50);
        for (Order order : {Order::LR2, Order::LR4, Order::LR8})
            for (double fc : {80.0, 500.0, 3000.0})
            {
                mm::LinkwitzRiley<double> xo;
                xo.prepare(sr);
                xo.setOrder(order);
                xo.setCrossover(fc);
                std::vector<double> irLow(65536), irHigh(65536);
                for (int n = 0; n < 65536; ++n)
                {
                    double lo, hi;
                    xo.process(n == 0 ? 1.0 : 0.0, lo, hi);
                    irLow[static_cast<size_t>(n)] = lo;
                    irHigh[static_cast<size_t>(n)] = hi;
                }
                for (double f : freqs)
                {
                    INFO("order " << static_cast<int>(order) << " sr " << sr << " fc " << fc << " at " << f);
                    const auto mLo = mm::test::responseAt(irLow, sr, f), mHi = mm::test::responseAt(irHigh, sr, f);
                    const auto eLo = xo.evaluateLow(f), eHi = xo.evaluateHigh(f);
                    if (std::abs(eLo) > 1e-5)
                    {
                        REQUIRE(std::abs(mm::test::magDb(mLo) - mm::test::magDb(eLo)) < 0.1);
                        REQUIRE(std::abs(mm::test::phaseDiffDeg(mm::test::phaseDeg(mLo), mm::test::phaseDeg(eLo))) < 2.0);
                    }
                    if (std::abs(eHi) > 1e-5)
                    {
                        REQUIRE(std::abs(mm::test::magDb(mHi) - mm::test::magDb(eHi)) < 0.1);
                        REQUIRE(std::abs(mm::test::phaseDiffDeg(mm::test::phaseDeg(mHi), mm::test::phaseDeg(eHi))) < 2.0);
                    }
                    REQUIRE(std::abs(mm::test::magDb(mLo + mHi)) < 0.01); // phase-coherent all-pass sum
                }
                // -6.02 dB at the crossover, both outputs
                REQUIRE(mm::test::magDb(xo.evaluateLow(fc)) == Catch::Approx(-6.02).margin(0.05));
                REQUIRE(mm::test::magDb(xo.evaluateHigh(fc)) == Catch::Approx(-6.02).margin(0.05));
            }
    }
}

TEST_CASE("LR4 slope: below -22 dB one octave above the crossover")
{
    mm::LinkwitzRiley<double> xo;
    xo.prepare(48000.0);
    xo.setOrder(Order::LR4);
    xo.setCrossover(500.0);
    REQUIRE(mm::test::magDb(xo.evaluateLow(1000.0)) < -22.0);
    REQUIRE(mm::test::magDb(xo.evaluateHigh(250.0)) < -22.0);
    xo.setOrder(Order::LR8);
    REQUIRE(mm::test::magDb(xo.evaluateLow(1000.0)) < -46.0);
    xo.setOrder(Order::LR2);
    REQUIRE(mm::test::magDb(xo.evaluateLow(1000.0)) < -10.0);
}

TEST_CASE("Linkwitz-Riley edge crossovers are finite")
{
    for (double sr : mm::test::kMatrixRates)
        for (Order order : {Order::LR2, Order::LR4, Order::LR8})
            for (double fc : {20.0, 0.45 * sr})
            {
                mm::LinkwitzRiley<float> xo;
                xo.prepare(sr);
                xo.setOrder(order);
                xo.setCrossover(fc);
                for (int n = 0; n < 20000; ++n)
                {
                    float lo, hi;
                    xo.process(n == 0 ? 1.0f : 0.0f, lo, hi);
                    REQUIRE(std::isfinite(lo));
                    REQUIRE(std::isfinite(hi));
                }
            }
}
