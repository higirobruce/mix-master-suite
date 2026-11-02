# ADR-0009: De-esser topology (band / split), detector, lookahead, listen

**Status:** Accepted (2026-11-03)

## Context

Spec: De-esser with frequency 2–12 kHz, threshold, range, width (band/split mode), listen, lookahead; "band-split with Linkwitz–Riley or a dynamic bell; detector on the band only"; budget < 0.4 %, latency 0–3 ms.

## Decision

- **Detector** (both modes): the band around `freq_hz` (SVF band-pass with Q from `width_oct`: Q = 1 / (2 sinh(ln2/2 · width)) in Band mode; the LR4 high band in Split mode) → peak detector → lookahead sliding maximum (0–3 ms; the audio is delayed by the same amount and the latency reported) → reduction = min(range, max(0, level − threshold)) in dB (a de-esser clamps everything above the threshold down to it, capped by the range) → `EnvelopeFollower` on the reduction (attack 0.5 ms, release 60 ms). Stereo detection is linked (maximum over channels); one gain for both channels.
- **Band mode** applies the reduction as a dynamic bell: an SVF peak at `freq_hz` with the detector's Q and gain −reduction, on the full signal. With no reduction the bell is the identity, so the plugin nulls exactly.
- **Split mode** splits with an LR4 at `freq_hz` and scales the high band by the reduction; the low band passes. With no reduction the sum is the LR4 all-pass: magnitude-flat, not a sample null (recorded).
- **Listen** outputs dry − wet: exactly what the de-esser removed, in either mode.
- `lookahead_ms` is unsmoothed (an integer latency; hard rule 2 exception as for the gate): applied at block start before the dry copy, with a 5 ms crossfade between the old and new delay taps.

## Consequences

- Tests gate band isolation (a 1 kHz tone within 0.05 dB while 6 kHz is reduced), the range cap, flatness without reduction, listen = dry − wet, and lookahead latency at four rates.
- CPU: one SVF (or LR4) per channel for the detector, one SVF bell (or LR4 gain) per channel, a sliding max and an envelope: well inside 0.4 %.
