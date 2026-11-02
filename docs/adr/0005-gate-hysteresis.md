# ADR-0005: Gate / expander hysteresis, hold and lookahead

**Status:** Accepted (2026-11-01)

## Context

The spec's Gate / Expander: threshold, range, attack, hold, release, ratio (expander mode), sidechain HP/LP, lookahead 0–5 ms, external sidechain, hysteresis between open/close thresholds to avoid chatter; latency 0–5 ms reported; < 0.3 % CPU.

## Decision

- Detector chain as ADR-0004 (sidechain HP/LP, peak or RMS detector, `Lookahead` for 0–5 ms with the audio delayed by the same amount and the envelope taken as the sliding maximum over the window).
- Gate mode is a state machine on the detected level in dB: `Closed → Opening` when level ≥ threshold; `Open → Hold` when level < threshold − hysteresis_db; `Hold → Closing` after hold_ms unless retriggered (level ≥ threshold restarts the hold and returns to Open); `Closing → Closed` when the gain reaches range_db. Gains move with the attack (opening) and release (closing) 10–90 % times through an `EnvelopeFollower` on the gain, floor = `range_db`.
- Expander mode uses `GainComputer{expander, ratio, range}` with the same hysteresis applied to the threshold crossing and the same ballistics; knee not used (the spec lists none for the gate).
- Lookahead latency is reported via `dspLatencySamples()` and bounded by `maxDspLatencySamples()` = 5 ms.
- `listen_sc` routes the filtered sidechain to the output for tuning.

## Consequences

- The key level is a 2 ms sliding maximum (peak hold) of the detector, so a tone at or above 250 Hz reads as its amplitude rather than its instantaneous |x|; without it the level touches zero at every zero crossing and the hysteresis never engages. The close decision therefore lags the signal by 2 ms (measured hold = hold + 1 ms). Below 250 Hz the hold (default 50 ms, restarted on every peak ≥ threshold) bridges the cycles.
- Chatter is prevented by construction for any hysteresis > 0 on material the peak hold reads flat (tested: 1 kHz tone at the threshold with ±1 dB AM at 5 Hz: 0 open→close transitions at 3 dB hysteresis, 15 at 0 dB).
- The key is linked (maximum over the main channels); one gain is applied to every channel. The detector is peak only (a gate wants the transient); RMS keying is not offered.
- Hold restart on retrigger means fast repeated hits keep the gate open (the expected drum behaviour).

## Amendments (2026-11-02, M2 review fix pass)

- `lookahead_ms` is the one continuous parameter that is **not** smoothed (hard rule 2 exception): it is an integer latency. Changes are applied at the start of a block (`beginBlock`, before the dry copy, so the reported latency and the dry/bypass path move together) and the audio crossfades from the old delay tap to the new one over 5 ms, so a lookahead change never jumps the signal (measured: sample-to-sample steps within 1.5× the tone's own).
