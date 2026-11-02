# ADR-0003: Channel EQ band architecture and analog Q

**Status:** Accepted (2026-11-01)

## Context

The spec's Channel EQ: 7 bands (HP/LP 6–48 dB/oct, 2 shelves, 3 parametrics), per-band freq/gain/Q/type, analog or digital Q behaviour, spectrum analyser, matched-response biquads, optional 2× oversampling, 64-bit filters, < 0.5 % CPU, frequency response within ±0.1 dB / ±2° of the analytic curve. M1 measured float biquad coefficients deviating up to ~4 dB at 20 Hz / Q 10 / 192 kHz.

## Decision

- Bands: `hp` (6/12/18/24/36/48 dB/oct), `b1` low shelf (default), `b2`–`b4` bells, `b5` high shelf (default), `lp` (same slopes). Any of b1–b5 can be switched between Bell / LowShelf / HighShelf.
- Bells and shelves use `mm::matched` designs (Vicanek three-point match with the M1 Nyquist handling); HP/LP sections use `mm::rbj` Butterworth stages: 6 dB = `OnePoleHP/LP`, 12 = one biquad Q 0.7071, 18 = one-pole + biquad Q 1.0, 24 = biquads Q 0.5412 / 1.3066, 36 = one-pole + biquads Q 0.5176 / 0.7071 / 1.9319, 48 = biquads Q 0.5098 / 0.6013 / 0.9000 / 2.5629.
- All filters run `double` (coefficients and state); the audio buffer is converted per block. Cost: M1 bench shows float and double biquads at the same ns/sample.
- Analog Q ("proportional Q"): the effective Q of a bell scales with its gain, `Q_eff = Q · (1 + |gain_db| / 12)` (narrower when boosting or cutting harder, as on console EQs); Digital mode uses Q as set. Shelves ignore the mode.
- Coefficients are recomputed per block from the smoothed `freq`, `gain`, `Q` when any of them moved by more than 1e-4 of its range; state is kept across recomputation (TDF-II tolerates block-rate coefficient steps at 20 ms smoothing; verified by `test_eq_band_sweep_stability`).
- Optional 2× oversampling through `mm::Oversampler` around the whole band chain; latency reported to the host (`dspLatencySamples`); the oversampler is re-prepared on toggle (allowed: parameter change outside the audio callback is not guaranteed, so the processor keeps both a 1× and a 2× path prepared and switches the flag atomically).
- Analyser: `mm::SpectrumAnalyser` (4096-point, pre and/or post) fed from the audio thread, drawn behind the curve.
- `evaluateAt(hz)` returns the product of every active section's transfer function for the curve display and the tests.

## Consequences

- Nyquist-cramping is avoided for bells/shelves up to ~0.45 fs; above that the matched design degrades continuously (M1).
- Double filters cost nothing measurable over float here and remove the low-frequency precision risk.
- The 1×/2× dual path doubles filter state memory (negligible) and keeps the oversampling toggle click-free.

## Amendments (2026-11-02, M2 review fix pass)

- The dual filter bank for a click-free oversampling toggle was not built: the toggle (and the Q-mode switch) is a configuration change that resets the filter state and applies the new design directly (measured −37 dBFS click on a tone). Both are applied at the start of the block (`beginBlock`), so the reported latency and the dry/bypass path change together.
- Switches are phase-continuous glides, not crossfades: enabling/disabling a band glides its gain to 0 dB over 20 ms; enabling/disabling an HP/LP (and changing its slope) glides the cutoff to the band edge (2 Hz / 0.49 fs) over 20 ms, then the section leaves the chain. A dry/wet crossfade of a phase-shifting section dips (−1.6 dB at fc) and was rejected.
- Band gain designs never hit exactly 0 dB (±0.001 dB keeps the poles): interpolating biquad coefficients towards the identity collapses the gain within ~2 ms.
- HP/LP sections are TPT state-variable filters (same bilinear transfer function as the RBJ design); the TDF-II biquad released stored energy as a +5.5 dB transient when a 48 dB/oct cutoff moved quickly. Cutoff glides are redesigned every 8 samples on a log-frequency path. Bands stay TDF-II biquads (their coefficient interpolation is linear in amplitude once the poles stay put).
- The editor never touches DSP state: the curve is designed from parameter values on the message thread (`staticResponseAt`), and drags are wrapped in host automation gestures.
