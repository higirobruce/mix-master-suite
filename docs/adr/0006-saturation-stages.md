# ADR-0006: Saturation characters, fixed 4× oversampling, and the alias gate

**Status:** Accepted (2026-11-02)

## Context

Spec: Saturation / Console with drive, character (tape / tube / transistor / clip), bias (even/odd balance), HF roll-off, mix, output; "nonlinear library with ADAA at 4× oversampling; tape mode adds hysteresis + head-bump EQ"; budget < 1.0 % per instance, latency ~1 ms, 4× fixed. Testing row: "19 kHz tone through saturation at full drive: alias products below −80 dBFS with oversampling on."

M1 measured (tanh, 19 kHz at −12 dBFS): 4×+ADAA1 at +6 dB drive −80/−83/−112 dBFS (44.1/48/96 k); at +18 dB −67/−44/−68; 8×+ADAA1 at +12 dB −91/−95/−100. Jiles–Atherton tape hysteresis costs 311 ns/sample; at 4× for a stereo instance at 48 k that is ~12 % of a core against the 1 % budget.

## Decision

- **Fixed 4× oversampling** (spec) with first-order ADAA on the static shapers; latency = the oversampler's, reported.
- **Characters** (all unity small-signal gain: the stage output is divided by the shaper's slope at zero, so drive adds colour, not level):
  - Tape: pre-emphasis/de-emphasis one-pole pair (+3 dB at 10 kHz before the shaper, inverse after), `atan` shaper through ADAA, +2 dB low shelf at 60 Hz (head bump) and the HF roll-off. **Static model**: the Jiles–Atherton hysteresis is not used (cost above); a hysteresis option returns when a cheaper solver exists.
  - Tube: `TubeTransfer` (asymmetric grid curve, bias = operating point) at 4× without ADAA (no closed-form antiderivative); its alias floor is recorded.
  - Transistor: `tanh` through ADAA, asymmetry from the bias control.
  - Clip: soft clip (knee 0.5) through ADAA.
- **Bias** shifts the operating point into the shaper and subtracts the resulting DC (`shaper(x + b) − shaper(b)`), so even harmonics scale with |bias| and the output stays DC-free.
- **Alias gate ruling.** The spec's "full drive" is unattainable: a 19 kHz tone driven to a square wave folds its 9th/11th harmonics into the band at −20 dB relative, at 4× or 8×. The gate is therefore asserted at the **reference drive** of each character, defined as the drive at which a 1 kHz tone at −12 dBFS produces 1 % THD (mild colour, the common setting), at 44.1/48/96/192 k. The alias floor at +12 dB drive and at maximum drive (36 dB) is recorded per character and rate in STATE.md and raised to the reviewer. This keeps the spec question from M1 open rather than silently redefining it.

## Consequences

- Saturation meets the budget (ADAA1 at 4×: ~13 ns × 4 × 2 channels plus the oversampler) with headroom for the EQ stages.
- Tape has no genuine hysteresis loop; the head bump and emphasis give the tonal signature. Reviewer may ask for the Jiles–Atherton option behind an "HQ" switch in a later milestone.
- The harmonic display in the editor computes from the same static stage on the GUI thread (a private copy; no audio-thread state is read).
