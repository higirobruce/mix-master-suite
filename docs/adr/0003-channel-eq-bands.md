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
