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

## Amendment (2026-11-03, Task 4 measurements)

- **All-pass legs are single biquads.** An LR4's low + high is exactly the second-order all-pass at its cutoff with Q = 1/√2 (LP² + HP² = D̄/D), so the mid path is two all-pass biquads and the side's low band passes through one all-pass biquad at the upper crossover (so the three-band side sum equals the two-stage all-pass that the mid sees). Only the side's two splitting crossovers and the mono-below filter are full LR4 networks: 16 biquads per sample instead of 28.
- **64-bit crossovers** (as the spec's budget table gives the multiband compressor): float LR4s at 150 Hz / 192 kHz left the width-100 null at −83 dBFS; in double it is below −100 dBFS at all four rates.
- **Mono-below engagement.** `mono_below_hz` is unsmoothed (ADR-0002 exception, like the de-esser lookahead): switching on from 0 starts the filters at the target cutoff from reset and crossfades them in over 20 ms; switching off crossfades out and then stops them; moves while on glide at 5 ms per octave (per block). A smoothed cutoff ramp from 0 Hz drove TDF-II LR4s through a 5 → 200 Hz sweep in 20 ms and clicked (0.17 peak on a −12 dBFS tone).
- **Test geometry.** Band tones sit ≥ 2 octaves inside their band (40 / 800 / 12 000 Hz): the LR4 edge is −0.22 dB at 1.3 octaves, so a 60 Hz tone against a 150 Hz crossover reads +5.9 dB at width 200, not +6.02. Mono-below is measured at 2.9 octaves below its frequency (< −60 dB); at 1.7 octaves (60 Hz under 200 Hz) the LR4 leaves −42 dB, which is recorded, not failed.
- Pan and output gains are computed at block edges and ramped per sample (constant power holds at the edges; the deviation inside a block during automation is below 0.01 dB).

## Amendment 2 (2026-11-03, M4 whole-branch review)

- **Pan is a constant-power stereo rotation** by up to ±45° (L' = L cos θ − R sin θ, R' = L sin θ + R cos θ, θ = pan · 45°), not a per-channel balance. A mono source follows exactly the previous sin/cos law (+3 dB on the kept side at full pan); stereo content moves instead of losing a channel (the review found a hard-right source vanishing at pan −100). Content already at the far edge rotates past it into anti-phase at full pan, as with any rotation; reviewer question.
- **The mono-below glide steps every 16 samples inside the loop** (5 ms per octave regardless of block size; per block it was 2.1 octaves per step at 512 samples and clicked).
- **Mono-below stays engaged across a reset:** `resetDsp` re-engages at full mix when the parameter is ≥ 5 Hz, so a transport reset or rate change does not pass 20 ms of side bass.
