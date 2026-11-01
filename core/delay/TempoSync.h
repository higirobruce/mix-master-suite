#pragma once

namespace mm::TempoSync
{
// Length of a note value in seconds. division: 1 = whole, 4 = quarter, 8 = eighth ... dotted x1.5, triplet x2/3.
inline double noteSeconds(double bpm, int division, bool dotted, bool triplet) noexcept
{
    if (bpm <= 0.0 || division <= 0)
        return 0.0;
    double s = 60.0 / bpm * 4.0 / static_cast<double>(division);
    if (dotted)
        s *= 1.5;
    if (triplet)
        s *= 2.0 / 3.0;
    return s;
}
inline double noteMs(double bpm, int division, bool dotted, bool triplet) noexcept
{
    return 1000.0 * noteSeconds(bpm, division, dotted, triplet);
}
} // namespace mm::TempoSync
