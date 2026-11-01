#include "core/metering/PeakMeter.h"
#include "core/metering/RmsMeter.h"
#include "helpers/Signals.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("peak and RMS meters read a 0 dBFS sine")
{
    const double sr = 48000.0;
    auto x = mm::test::sine(1000.0, sr, 1.0);
    mm::PeakMeter peak;
    mm::RmsMeter rms;
    peak.prepare(sr);
    rms.prepare(sr);
    for (size_t i = 0; i + 64 <= x.size(); i += 64)
    {
        peak.process(x.data() + i, 64);
        rms.process(x.data() + i, 64);
    }
    REQUIRE(peak.levelDb() == Catch::Approx(0.0).margin(0.01));
    REQUIRE(rms.levelDb() == Catch::Approx(-3.01).margin(0.05));

    std::vector<float> silence(64, 0.0f);
    for (int i = 0; i < static_cast<int>(1.1 * sr / 64); ++i)
        peak.process(silence.data(), 64);
    REQUIRE(peak.levelDb() < -20.0);
}
