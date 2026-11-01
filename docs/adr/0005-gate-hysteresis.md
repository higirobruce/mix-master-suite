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

- Chatter is prevented by construction for any hysteresis > 0 (tested: tone at the threshold with ±1 dB AM at 5 Hz, ≤ 1 transition per cycle at 3 dB hysteresis).
- Hold restart on retrigger means fast repeated hits keep the gate open (the expected drum behaviour).
