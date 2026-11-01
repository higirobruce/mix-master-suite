#include "core/filters/FIRDesign.h"
#include "core/filters/FIRFilter.h"
#include "helpers/Response.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numeric>

TEST_CASE("windowed-sinc design: unity DC, symmetric, Kaiser 255 taps meets -80 dB stopband and 0.01 dB ripple")
{
    const auto taps = mm::fir::windowedSinc(255, 0.25, mm::fir::Window::Kaiser, 8.6);
    REQUIRE(taps.size() == 255);
    REQUIRE(std::accumulate(taps.begin(), taps.end(), 0.0) == Catch::Approx(1.0).margin(1e-6));
    for (size_t i = 0; i < taps.size(); ++i)
        REQUIRE(taps[i] == Catch::Approx(taps[taps.size() - 1 - i]).margin(1e-15));
    const double sr = 1.0; // normalised frequencies
    for (double f = 0.0; f <= 0.22; f += 0.005)
        REQUIRE(std::abs(mm::test::magDb(mm::test::responseAt(taps, sr, f))) < 0.01);
    for (double f = 0.3; f <= 0.5; f += 0.005)
        REQUIRE(mm::test::magDb(mm::test::responseAt(taps, sr, f)) < -80.0);
}

TEST_CASE("half-band design has zero even taps except the centre")
{
    const auto taps = mm::fir::halfBand(63, 0.1);
    REQUIRE(taps.size() == 63);
    const size_t centre = 31;
    REQUIRE(taps[centre] == Catch::Approx(0.5).margin(1e-9));
    for (size_t i = 1; i < taps.size(); i += 2) // odd offsets from the centre are the zero taps for a half-band
    {
        const size_t idx = centre + i;
        if (idx < taps.size() && i % 2 == 0)
            REQUIRE(taps[idx] == 0.0);
    }
    for (size_t i = 2; centre + i < taps.size(); i += 2)
        REQUIRE(taps[centre + i] == Catch::Approx(0.0).margin(1e-12));
    REQUIRE(std::abs(mm::test::magDb(mm::test::responseAt(taps, 1.0, 0.0))) < 0.001);
    REQUIRE(mm::test::magDb(mm::test::responseAt(taps, 1.0, 0.25)) == Catch::Approx(-6.02).margin(0.05));
}

TEST_CASE("FIR filter impulse response equals its taps and reports (n-1)/2 latency")
{
    const auto taps = mm::fir::windowedSinc(63, 0.2, mm::fir::Window::Hann);
    mm::FIRFilter<float> fir;
    fir.prepare(255, 2);
    fir.setTaps(taps.data(), static_cast<int>(taps.size()));
    REQUIRE(fir.latencySamples() == 31);
    for (int n = 0; n < 63; ++n)
        REQUIRE(fir.process(n == 0 ? 1.0f : 0.0f, 0) == Catch::Approx(taps[static_cast<size_t>(n)]).margin(1e-7));
    // group delay flat: phase slope = -(n-1)/2 samples
    std::vector<double> t(taps.begin(), taps.end());
    const double sr = 48000.0;
    for (double f : {500.0, 2000.0, 6000.0})
    {
        const auto h = mm::test::responseAt(t, sr, f);
        const double expectedPhase = -360.0 * f / sr * 31.0;
        REQUIRE(std::abs(mm::test::phaseDiffDeg(mm::test::phaseDeg(h), std::fmod(expectedPhase, 360.0))) < 0.01);
    }
}

TEST_CASE("float FIR path nulls against the double reference below -120 dBFS")
{
    const auto taps = mm::fir::windowedSinc(63, 0.3, mm::fir::Window::Blackman);
    mm::FIRFilter<float> ff;
    ff.prepare(63, 1);
    ff.setTaps(taps.data(), 63);
    std::vector<double> hist(63, 0.0);
    double worst = 0.0;
    for (int n = 0; n < 20000; ++n)
    {
        const float x = 0.5f * std::sin(0.37f * static_cast<float>(n));
        hist.insert(hist.begin(), x);
        hist.pop_back();
        double ref = 0.0;
        for (int k = 0; k < 63; ++k)
            ref += taps[static_cast<size_t>(k)] * hist[static_cast<size_t>(k)];
        worst = std::max(worst, std::abs(static_cast<double>(ff.process(x, 0)) - ref));
    }
    REQUIRE(20.0 * std::log10(worst + 1e-30) < -120.0);
}

TEST_CASE("FIR setTaps clamps to the prepared maximum")
{
    const auto taps = mm::fir::windowedSinc(127, 0.2, mm::fir::Window::Hamming);
    mm::FIRFilter<float> fir;
    fir.prepare(63, 1);
    fir.setTaps(taps.data(), 127);
    REQUIRE(fir.numTaps() == 63);
    for (int n = 0; n < 200; ++n)
        REQUIRE(std::isfinite(fir.process(1.0f, 0)));
}
