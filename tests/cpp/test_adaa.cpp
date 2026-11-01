#include "core/common/Math.h"
#include "core/nonlinear/ADAA.h"
#include "core/nonlinear/Waveshapers.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("ADAA ill-conditioned fallback: DC input returns the plain function exactly")
{
    mm::ADAA1<mm::shape::tanhS, mm::shape::tanhAD1> adaa;
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
    mm::ADAA1<mm::shape::hardClip, mm::shape::hardClipAD1> adaa;
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
