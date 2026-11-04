#include "core/filters/BiquadDesign.h"
#include "core/filters/Transfer.h"
#include "core/fir/LinearPhaseDesign.h"
#include "helpers/Matrix.h"
#include "helpers/Response.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <complex>
#include <numbers>

namespace
{
// the target the design must reproduce: the product of the bands' RBJ biquad magnitudes
double targetDb(const std::vector<mm::fir::BandSpec>& bands, double sr, double hz)
{
    double db = 0.0;
    for (const auto& b : bands)
    {
        mm::BiquadCoeffs<double> c;
        switch (b.type)
        {
        case mm::FilterType::Peak: c = mm::rbj::peak(sr, b.hz, b.gainDb, b.q); break;
        case mm::FilterType::LowShelf: c = mm::rbj::lowShelf(sr, b.hz, b.gainDb, b.q); break;
        case mm::FilterType::HighShelf: c = mm::rbj::highShelf(sr, b.hz, b.gainDb, b.q); break;
        case mm::FilterType::LowPass: c = mm::rbj::lowPass(sr, b.hz, b.q); break;
        case mm::FilterType::HighPass: c = mm::rbj::highPass(sr, b.hz, b.q); break;
        case mm::FilterType::Notch: c = mm::rbj::notch(sr, b.hz, b.q); break;
        case mm::FilterType::AllPass: c = mm::rbj::allPass(sr, b.hz, b.q); break;
        }
        db += 20.0 * std::log10(std::abs(mm::evaluate(c, sr, hz)));
    }
    return db;
}
} // namespace

TEST_CASE("linear-phase design of no bands is a centred unit impulse")
{
    for (int length : {2048, 16384})
    {
        const auto h = mm::fir::design(std::vector<mm::fir::BandSpec>{}, length, 48000.0);
        REQUIRE(static_cast<int>(h.size()) == length - 1);
        const size_t centre = h.size() / 2;
        for (size_t i = 0; i < h.size(); ++i)
            REQUIRE(h[i] == Catch::Approx(i == centre ? 1.0 : 0.0).margin(1e-12));
    }
}

TEST_CASE("linear-phase design matches the band product above twice the resolution floor, phase linear, at four rates")
{
    // every band's frequency sits above the floor: the high-pass follows it at the short length / high rates
    for (double sr : mm::test::kMatrixRates)
        for (int length : {2048, 16384})
        {
            const double floor = mm::fir::resolutionHz(length, sr);
            REQUIRE(floor == Catch::Approx(2.0 * sr / length));
            const std::vector<mm::fir::BandSpec> bands = {{mm::FilterType::LowShelf, 300.0, 0.7071, 6.0},
                                                           {mm::FilterType::Peak, 1000.0, 1.0, -4.0},
                                                           {mm::FilterType::HighShelf, 6000.0, 0.7071, 3.0},
                                                           {mm::FilterType::HighPass, std::max(40.0, 2.0 * floor), 0.7071, 0.0}};
            const auto h = mm::fir::design(bands, length, sr);
            REQUIRE(static_cast<int>(h.size()) == length - 1);
            const int centre = (length - 2) / 2;
            const double lo = 2.0 * floor, hi = 0.45 * sr;
            for (int i = 0; i <= 200; ++i)
            {
                const double f = lo * std::pow(hi / lo, i / 200.0);
                const auto H = mm::test::responseAt(h, sr, f);
                const double gotDb = 20.0 * std::log10(std::abs(H));
                INFO("sr " << sr << " length " << length << " f " << f);
                REQUIRE(gotDb == Catch::Approx(targetDb(bands, sr, f)).margin(0.1));
                // remove the centre delay: what is left must be real and positive (zero phase)
                const auto zeroPhase = H * std::polar(1.0, 2.0 * std::numbers::pi * f / sr * centre);
                const double phaseDeg = std::arg(zeroPhase) * 180.0 / std::numbers::pi;
                REQUIRE(std::abs(phaseDeg) < 2.0);
            }
        }
}

TEST_CASE("linear-phase design renders a band below the floor smeared, not ringing: DC within 1.5 dB, 0.1 dB above four floors")
{
    // 2048 taps at 192 k: floor 187.5 Hz; a +6 dB low shelf at 30 Hz cannot be resolved: the taper smears it (DC
    // reads ~4.9 dB) instead of letting the truncation ring across the spectrum (the EQ shows the floor; ADR-0011)
    const double sr = 192000.0;
    const int length = 2048;
    const double floor = mm::fir::resolutionHz(length, sr);
    const std::vector<mm::fir::BandSpec> bands = {{mm::FilterType::LowShelf, 30.0, 0.7071, 6.0}, {mm::FilterType::Peak, 2000.0, 1.0, 3.0}};
    const auto h = mm::fir::design(bands, length, sr);
    REQUIRE(20.0 * std::log10(std::abs(mm::test::responseAt(h, sr, 0.0))) == Catch::Approx(6.0).margin(1.5));
    for (int i = 0; i <= 100; ++i)
    {
        const double f = 4.0 * floor * std::pow(0.45 * sr / (4.0 * floor), i / 100.0);
        INFO("f " << f);
        REQUIRE(20.0 * std::log10(std::abs(mm::test::responseAt(h, sr, f))) == Catch::Approx(targetDb(bands, sr, f)).margin(0.1));
    }
}

TEST_CASE("linear-phase design is exactly symmetric")
{
    const std::vector<mm::fir::BandSpec> bands = {{mm::FilterType::Peak, 2000.0, 3.0, 9.0}, {mm::FilterType::LowPass, 12000.0, 0.7071, 0.0}};
    const auto h = mm::fir::design(bands, 4096, 44100.0);
    for (size_t i = 0; i < h.size(); ++i)
        REQUIRE(h[i] == h[h.size() - 1 - i]);
}

#include "core/filters/MatchedBiquad.h"
TEST_CASE("linear-phase design from biquad coefficients equals the band-spec design and takes any design family")
{
    const double sr = 48000.0;
    const std::vector<mm::fir::BandSpec> bands = {{mm::FilterType::Peak, 1000.0, 1.0, -4.0}, {mm::FilterType::HighShelf, 6000.0, 0.7071, 3.0}};
    std::vector<mm::BiquadCoeffs<double>> coeffs;
    for (const auto& b : bands)
        coeffs.push_back(mm::fir::bandCoeffs(b, sr));
    const auto a = mm::fir::design(bands, 4096, sr);
    const auto b = mm::fir::design(coeffs, 4096, sr);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i)
        REQUIRE(a[i] == b[i]);
    // a matched (Vicanek) bell: the FIR follows that magnitude, not the RBJ one
    const auto matched = mm::matched::peak(sr, 12000.0, 9.0, 2.0);
    const auto h = mm::fir::design(std::vector<mm::BiquadCoeffs<double>>{matched}, 16384, sr);
    for (double f : {8000.0, 12000.0, 16000.0, 20000.0})
        REQUIRE(20.0 * std::log10(std::abs(mm::test::responseAt(h, sr, f))) == Catch::Approx(20.0 * std::log10(std::abs(mm::evaluate(matched, sr, f)))).margin(0.1));
}
