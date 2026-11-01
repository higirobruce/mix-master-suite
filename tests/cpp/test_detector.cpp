#include "core/dynamics/Detector.h"
#include "helpers/Matrix.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("peak detector returns |x|; RMS detector settles to 0.7071 for a unit sine")
{
    for (double sr : mm::test::kMatrixRates)
    {
        mm::Detector<float> peak, rms;
        peak.prepare(sr);
        peak.setMode(mm::DetectorMode::Peak);
        rms.prepare(sr);
        rms.setMode(mm::DetectorMode::Rms);
        rms.setRmsWindowMs(50.0);
        float lastRms = 0.0f;
        for (int n = 0; n < static_cast<int>(sr); ++n)
        {
            const float x = static_cast<float>(std::sin(2.0 * 3.14159265358979 * 1000.0 * n / sr));
            REQUIRE(peak.process(0.5f * x) == Catch::Approx(0.5f * std::abs(x)).margin(1e-7));
            lastRms = rms.process(x);
        }
        REQUIRE(lastRms == Catch::Approx(0.70710678f).margin(0.0036f)); // 0.5 %
    }
}
