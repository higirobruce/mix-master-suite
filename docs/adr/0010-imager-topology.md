# ADR-0010: Stereo imager topology (three-band M/S width, mono-below, pan, correlation)

**Status:** Accepted (2026-11-03)

## Context

Spec: Stereo imager with width 0–200 % per band (3 bands), pan, mono below frequency, correlation meter; "M/S encode → per-band gain → decode; Linkwitz–Riley band split"; budget < 0.4 %, latency 0; testing: mono-compatibility and correlation.

## Decision

- M/S encode (M = (L+R)/2, S = (L−R)/2). The side is split into three bands by two LR4 crossovers (`xover_low_hz`, `xover_high_hz`); each band is scaled by its width (0–200 %) and the bands are summed. **The mid passes through the same two LR4 networks (low + high summed = the LR4 all-pass)** so M and S keep the same phase response and the stereo image stays coherent at every width. Mono-below removes the side below `mono_below_hz` with a third LR4 on S (0 = off). Decode, then a constant-power pan and the output gain.
- **Mono compatibility by construction:** L + R = 2 M, and M is never scaled by the width, so the mono sum does not change with width (only the all-pass phase, identical at every width). Width 0 gives L = R (correlation 1); width 200 doubles the side (+6.02 dB).
- **Width 100 is not a sample null.** An LR4 split sums to an all-pass; the output at width 100 is the all-passed input (magnitude-flat, L/R relationship intact). An exact null needs linear-phase crossovers, which arrive with M5's FIR engine; whether the imager should adopt them is a reviewer question.
- The correlation meter is the core `CorrelationMeter` (300 ms window) on the output, exposed to the editor with a stereo sample ring for the goniometer.

## Consequences

- Tests: width 0 → correlation 1.0; width 100 → flat ±0.02 dB and null < −100 dBFS against the `lr_crossover` all-passed reference at four rates; width 200 → side +6.02 dB per band; mono-below → side below f < −60 dB; L+R unchanged ±0.1 dB for every width; pan laws; correlation meter ±0.02 on known pairs.
- CPU: four LR4 networks (two on S, two on M) plus the mono-below LR4 per instance: ~0.15 % at 48 k.
