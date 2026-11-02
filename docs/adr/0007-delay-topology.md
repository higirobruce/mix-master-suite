# ADR-0007: Delay topology, time-change slew, feedback limit, ducking

**Status:** Accepted (2026-11-02)

## Context

Spec: Delay with time (ms or tempo sync, L/R independent), feedback, ping-pong, filter in the feedback path, modulation depth/rate, mix, ducking; "delay line + feedback filter; ducking uses the dynamics core keyed from the dry input"; budget < 0.5 %, latency 0. M1 provides `FractionalDelayLine` (Lagrange-3 / Thiran-1, slewed time changes) and `TempoSync`.

## Decision

- Two independent lines (L, R) of one `FractionalDelayLine<float>` prepared for 4.5 s at the host rate, Lagrange-3. Delay time in samples is derived from seconds on every block, so a sample-rate change keeps the labelled time.
- **Time changes** are not parameter-smoothed (the line's own 20 ms slew moves the read position; a smoothed parameter on top would double-slew): `time_l_ms`, `time_r_ms` carry `smoothing_ms: 0` (hard rule 2 exception, like the gate's lookahead). Tempo sync replaces the time with `noteMs(bpm, division, dotted, triplet)`; BPM comes from the host play head at block start, 120 when absent.
- **Feedback path**: wet → SVF low-pass → SVF high-pass → soft limit `2·tanh(x/2)` (ceiling +6 dBFS, transparent below −12 dBFS) → line input. Feedback up to 120 % is allowed for self-oscillation; the soft limit bounds it. Ping-pong crosses the feedback (L → R, R → L) and feeds the mono sum into L only, so a mono source alternates.
- **Modulation**: per-channel sine LFO (R at 90°) adds `depth_ms · sin` to the read time through the slewed target; the slew is disabled while depth > 0 (the LFO is the slew).
- **Ducking**: `EnvelopeFollower` (attack 5 ms, release `duck_release_ms`) on the dry peak level; wet gain = `−duck_amount_db · clamp((env + 40) / 40, 0, 1)` dB.
- Mix is a linear wet/dry blend at the host rate; the dry path has zero latency.

## Consequences

- A 1 → 4000 ms time jump glides over 20 ms (pitch sweep), never clicks; the test asserts sample-to-sample jumps within 2× the input's own.
- Self-oscillation never exceeds +6 dBFS.
- The render host gains a `bpm` job key and a play head so tempo sync is testable.
