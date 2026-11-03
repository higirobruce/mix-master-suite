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

## Amendment (2026-11-03, M4 whole-branch review)

- **Mono input on a stereo bus:** each filter runs once per input channel; extra outputs copy channel 0 (the review found every filter running twice per sample in a 1 → 2 layout, which left 0.8 dB of the 18 dB reduction).
- **Split-mode Listen** outputs the removed high band `hi · (1 − g)`, not `dry − wet` (the split is an all-pass, so the difference carried almost the whole signal when nothing was being removed). Listen + wet rebuilds the input in Band mode and the LR4 all-passed input in Split mode.
- **Mode and Listen are crossfaded** over 10 ms: on a mode change the incoming filter set starts from reset and both sets run during the fade; Listen fades between wet and the removed part. A change requested during a fade is taken when the fade ends.
- **Lookahead changes that land inside the 5 ms tap crossfade wait for it** (the previous code restarted the fade from the original tap and dropped the half-faded one: a 3.4–9.5× sample jump under automation every 1.4–3 ms). The reported latency stays on the tap being heard until the next change is taken, so the host sees the new latency at most 5 ms late. The gate shares the pattern and got the same fix.
- **CPU (2026-11-03, CI gate):** the dry copy goes through the lookahead ring in block copies except during the 5 ms tap crossfade; the bell is redesigned every 8 samples through `SVF::setParamsA` (linear A, cached tan); the sliding max is a block decomposition. 0.348 % on the reference machine (0.377 before).
