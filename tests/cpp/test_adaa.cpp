#include "core/common/Math.h"
#include "core/nonlinear/ADAA.h"
#include "core/nonlinear/Waveshapers.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("ADAA ill-conditioned fallback: DC input returns the plain function exactly")
{
    mm::ADAA1<mm::shape::tanhS, mm::shape::tanhAD1D> adaa;
    float y = 0.0f;
    for (int n = 0; n < 1000; ++n)
        y = adaa.process(0.3f);
    REQUIRE(std::isfinite(y));
    REQUIRE(y == Catch::Approx(mm::shape::tanhS(0.3f)).margin(1e-5));
}

TEST_CASE("ADAA lowers hard-clip aliasing by at least 12 dB at 2x-equivalent rates")
{
    // 19 kHz at +12 dB through hardClip at 96 kHz (2x of 48 k): compare alias floor below 19 kHz with and without ADAA
    const double sr = 96000.0;
    auto x = mm::test::sine(19000.0, sr, 1.0, mm::dbToGain(12.0f));
    std::vector<float> plain(x.size()), aa(x.size());
    mm::ADAA1<mm::shape::hardClip, mm::shape::hardClipAD1D> adaa;
    for (size_t i = 0; i < x.size(); ++i)
    {
        plain[i] = mm::shape::hardClip(x[i]);
        aa[i] = adaa.process(x[i]);
    }
    auto worst = [&](const std::vector<float>& y) {
        double w = -200.0;
        for (double f = 100.0; f < 18700.0; f += 50.0)
            w = std::max(w, mm::test::levelDbWindowed(y, sr, f, y.size() / 2, y.size()));
        return w;
    };
    const double p = worst(plain), a = worst(aa);
    INFO("plain " << p << " dBFS, ADAA " << a << " dBFS");
    REQUIRE(a < p - 12.0);
}


TEST_CASE("ADAA tanh in the float path matches an all-double ADAA within -100 dBFS on bass")
{
    // In float the antiderivative difference cancels to the ulp on slowly moving bass and left a -46 dBFS peak error.
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0})
        for (double f0 : {50.0, 100.0})
            for (float amp : {0.1f, 0.5f})
            {
                auto x = mm::test::sine(f0, sr, 0.5, amp);
                mm::ADAA1<mm::shape::tanhS, mm::shape::tanhAD1D> adaa;
                double x1 = 0.0, worst = 0.0;
                for (size_t i = 0; i < x.size(); ++i)
                {
                    const float y = adaa.process(x[i]);
                    const double xd = x[i], d = xd - x1;
                    const double ref = std::abs(d) < 1e-6 ? std::tanh(0.5 * (xd + x1)) : (mm::shape::tanhAD1D(xd) - mm::shape::tanhAD1D(x1)) / d;
                    x1 = xd;
                    worst = std::max(worst, std::abs(static_cast<double>(y) - ref));
                }
                INFO("sr " << sr << " f0 " << f0 << " amp " << amp << " worst error " << 20.0 * std::log10(worst + 1e-30) << " dBFS");
                REQUIRE(20.0 * std::log10(worst + 1e-30) < -100.0);
            }
}

#include "core/nonlinear/ADAATable.h"
TEST_CASE("tabulated ADAA matches the analytic antiderivative to 1e-9 and the analytic ADAA output to 1e-6")
{
    using Tab = mm::TabulatedADAA1<mm::shape::tanhS, mm::shape::tanhAD1D>;
    for (int i = 0; i <= 2000; ++i)
    {
        const double x = -60.0 + 120.0 * i / 2000.0 + 0.0037;
        REQUIRE(std::abs(Tab::antiderivative(x) - mm::shape::tanhAD1D(x)) < 1e-9);
    }
    Tab tab;
    mm::ADAA1<mm::shape::tanhS, mm::shape::tanhAD1D> exact;
    tab.reset();
    for (int n = 0; n < 4000; ++n)
    {
        const float x = 3.0f * std::sin(0.07f * static_cast<float>(n)) + 0.5f * std::sin(1.3f * static_cast<float>(n));
        REQUIRE(std::abs(tab.process(x) - exact.process(x)) < 1e-6f);
    }
}
