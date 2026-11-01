#pragma once

namespace mm
{
// One block's worth of meter readings, pushed from the audio thread to the GUI through a MeterFifo.
struct MeterFrame
{
    float peakInDb[2] = {-120.0f, -120.0f};
    float peakOutDb[2] = {-120.0f, -120.0f};
    float rmsInDb[2] = {-120.0f, -120.0f};
    float rmsOutDb[2] = {-120.0f, -120.0f};
    float gainReductionDb = 0.0f;
    float correlation = 1.0f;
    float latencyMs = 0.0f;
};
} // namespace mm
