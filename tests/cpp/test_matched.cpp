#include "core/filters/Biquad.h"
#include "core/filters/BiquadDesign.h"
#include "core/filters/MatchedBiquad.h"
#include "core/filters/Transfer.h"
#include "helpers/Matrix.h"
#include "helpers/Response.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
using mm::FilterType;
mm::BiquadCoeffs<double> design(FilterType t, double sr, double f, double g, double q)
{
    switch (t)
    {
    case FilterType::Peak: return mm::matched::peak(sr, f, g, q);
    case FilterType::LowShelf: return mm::matched::lowShelf(sr, f, g, q);
    case FilterType::HighShelf: return mm::matched::highShelf(sr, f, g, q);
    case FilterType::LowPass: return mm::matched::lowPass(sr, f, q);
    default: return mm::matched::highPass(sr, f, q);
    }
}
const FilterType kTypes[] = {FilterType::Peak, FilterType::LowShelf, FilterType::HighShelf, FilterType::LowPass,
                             FilterType::HighPass};
} // namespace

TEST_CASE("matched biquad implementation matches its analytic transfer function")
{
    for (double sr : mm::test::kMatrixRates)
    {
        const auto band = mm::test::measureBand(sr);
        const auto freqs = mm::test::logFrequencies(band.lowHz, band.highHz, 40);
        for (FilterType t : kTypes)
            for (double f : {100.0, 1000.0, 10000.0})
                for (double g : {-12.0, 6.0})
                    for (double q : {0.5, 2.0})
                    {
                        const auto c = design(t, sr, f, g, q);
                        mm::Biquad<double> bq;
                        bq.setCoeffs(c);
                        const auto ir = mm::test::impulseResponse([&](double x) { return bq.process(x); }, 65536);
                        for (double fm : freqs)
                        {
                            const auto m = mm::test::responseAt(ir, sr, fm);
                            const auto e = mm::evaluate(c, sr, fm);
                            INFO("type " << static_cast<int>(t) << " sr " << sr << " f0 " << f << " g " << g << " q " << q << " at " << fm);
                            REQUIRE(std::abs(mm::test::magDb(m) - mm::test::magDb(e)) < 0.1);
                            REQUIRE(std::abs(mm::test::phaseDiffDeg(mm::test::phaseDeg(m), mm::test::phaseDeg(e))) < 2.0);
                        }
                    }
    }
}

TEST_CASE("matched designs equal the analog prototype magnitude at DC, Nyquist and f0")
{
    for (double sr : mm::test::kMatrixRates)
        for (FilterType t : {FilterType::Peak, FilterType::LowShelf, FilterType::HighShelf})
            for (double f : {100.0, 2000.0, 12000.0})
                for (double g : {-9.0, 9.0})
                    for (double q : {0.7, 3.0})
                    {
                        const auto c = design(t, sr, f, g, q);
                        for (double hz : {0.0, f, 0.5 * sr})
                        {
                            const double digital = 20.0 * std::log10(std::abs(mm::evaluate(c, sr, hz)) + 1e-300);
                            const double analog = 20.0 * std::log10(std::abs(mm::analogPrototype(t, f, g, q, hz)) + 1e-300);
                            INFO("type " << static_cast<int>(t) << " sr " << sr << " f0 " << f << " g " << g << " q " << q << " at " << hz);
                            REQUIRE(digital == Catch::Approx(analog).margin(1e-6));
                        }
                    }
}

TEST_CASE("matched peak tracks the analog curve near Nyquist where RBJ does not")
{
    const double sr = 44100.0, f0 = 10000.0, q = 1.0, g = 6.0;
    const auto matched = mm::matched::peak(sr, f0, g, q);
    const auto rbj = mm::rbj::peak(sr, f0, g, q);
    double worstMatched = 0.0, worstRbj = 0.0;
    for (double hz = 100.0; hz <= 20000.0; hz *= 1.05)
    {
        const double analog = 20.0 * std::log10(std::abs(mm::analogPrototype(FilterType::Peak, f0, g, q, hz)));
        worstMatched = std::max(worstMatched, std::abs(20.0 * std::log10(std::abs(mm::evaluate(matched, sr, hz))) - analog));
        worstRbj = std::max(worstRbj, std::abs(20.0 * std::log10(std::abs(mm::evaluate(rbj, sr, hz))) - analog));
    }
    INFO("worst |matched - analog| " << worstMatched << " dB, |rbj - analog| " << worstRbj << " dB");
    REQUIRE(worstMatched < 0.5);
    REQUIRE(worstRbj > 0.5);
}

TEST_CASE("matched designs are finite and bounded at the edges")
{
    for (double sr : mm::test::kMatrixRates)
        for (FilterType t : kTypes)
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
                    REQUIRE(peak < 20.0f);
                }
}
