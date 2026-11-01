#include "core/filters/AllPass.h"
#include "core/filters/OnePole.h"
#include "helpers/Matrix.h"
#include "helpers/Response.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("one-pole LP/HP and all-pass sections match their analytic responses")
{
    for (double sr : mm::test::kMatrixRates)
    {
        const auto band = mm::test::measureBand(sr);
        const auto freqs = mm::test::logFrequencies(band.lowHz, band.highHz, 40);
        for (double fc : {30.0, 500.0, 6000.0})
        {
            mm::OnePoleHP<double> hp;
            hp.prepare(sr);
            hp.setCutoff(fc);
            mm::OnePoleLP<double> lp;
            lp.prepare(sr);
            lp.setCutoff(fc);
            mm::FirstOrderAllPass<double> ap1;
            ap1.prepare(sr);
            ap1.setCorner(fc);
            mm::SecondOrderAllPass<double> ap2;
            ap2.prepare(sr);
            ap2.setParams(fc, 0.8);
            const auto irHp = mm::test::impulseResponse([&](double x) { return hp.process(x); }, 65536);
            const auto irLp = mm::test::impulseResponse([&](double x) { return lp.process(x); }, 65536);
            const auto irAp1 = mm::test::impulseResponse([&](double x) { return ap1.process(x); }, 65536);
            const auto irAp2 = mm::test::impulseResponse([&](double x) { return ap2.process(x); }, 65536);
            for (double f : freqs)
            {
                INFO("sr " << sr << " fc " << fc << " at " << f);
                auto check = [&](std::complex<double> meas, std::complex<double> exp) {
                    if (std::abs(exp) < 1e-5)
                        return;
                    REQUIRE(std::abs(mm::test::magDb(meas) - mm::test::magDb(exp)) < 0.1);
                    REQUIRE(std::abs(mm::test::phaseDiffDeg(mm::test::phaseDeg(meas), mm::test::phaseDeg(exp))) < 2.0);
                };
                check(mm::test::responseAt(irHp, sr, f), hp.evaluate(f));
                check(mm::test::responseAt(irLp, sr, f), lp.evaluate(f));
                check(mm::test::responseAt(irAp1, sr, f), ap1.evaluate(f));
                check(mm::test::responseAt(irAp2, sr, f), ap2.evaluate(f));
                REQUIRE(std::abs(mm::test::magDb(mm::test::responseAt(irAp1, sr, f))) < 0.01);
                REQUIRE(std::abs(mm::test::magDb(mm::test::responseAt(irAp2, sr, f))) < 0.01);
            }
            // second-order all-pass: -180 degrees at the centre
            REQUIRE(mm::test::phaseDiffDeg(mm::test::phaseDeg(ap2.evaluate(fc)), -180.0) == Catch::Approx(0.0).margin(1.0));
        }
    }
}

TEST_CASE("one-pole HP at 10 Hz passes 100 Hz within 0.05 dB")
{
    for (double sr : mm::test::kMatrixRates)
    {
        mm::OnePoleHP<double> hp;
        hp.prepare(sr);
        hp.setCutoff(10.0);
        REQUIRE(std::abs(mm::test::magDb(hp.evaluate(100.0))) < 0.05);
        REQUIRE(mm::test::magDb(hp.evaluate(10.0)) == Catch::Approx(-3.01).margin(0.02));
    }
}
