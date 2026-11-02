# ADR-0004: Compressor topologies, ballistics and program-dependent release

**Status:** Accepted (2026-11-01)

## Context

The spec's Channel Compressor: feed-forward and feedback topologies selectable, program-dependent release, threshold, ratio 1:1–20:1 + ∞, attack 0.1–100 ms, release 10–2000 ms + auto, knee, peak/RMS, mix, sidechain HP, external key; gates: attack/release within 10 % of the labelled time, ratio within 0.1. M1 found that a log-domain follower on the detector level under-reads sine peaks (13 dB of reduction instead of 15 on a 1 kHz tone) and settled on ballistics applied to the gain reduction (Giannoulis, Massberg, Reiss 2012), measured on DC steps.

## Decision

- Chain (feed-forward): `SidechainFilter` (HP 12 dB/oct) → `Detector` (peak |x| or RMS) → dB → static `GainComputer` (threshold, ratio, knee; ∞ = ratio ≥ 1000) → `EnvelopeFollower` on the reduction (attack when reduction grows, release when it falls) → gain applied to the delayed-free audio; stereo link = blend between per-channel detector levels and their maximum (`link_pct`).
- Feedback topology: the detector reads the previous sample of the *output* (one-sample loop); the labelled ratio then produces a smaller measured ratio (characterised and shown as such in the UI tooltip), which is the point of the mode.
- Time labels are 10–90 % step times (`a = exp(-ln 9 / samples)`), gated on DC steps; tone-burst behaviour with the peak detector (settled reduction ~1.3 dB low, apparent attack ~1.6× at 1 kHz) is recorded, not gated.
- Auto release = M1's program-dependent release: a 60 ms stage blending into a 500 ms stage with the time the signal has spent above the envelope (0.5 s window).
- Makeup: manual −12…+24 dB; auto makeup = `|threshold| · (1 − 1/ratio) / 2`.
- Mix: parallel blend post-makeup, latency-neutral (no lookahead in this plugin).
- External key: the sidechain bus feeds the detector when `ext_key` is on and the bus is enabled; if the host disables the bus, the plugin falls back to the internal key silently.

## Consequences

- One detector chain shared by compressor and gate (ADR-0005) and later by bus/multiband compressors.
- Measured ratios on sines with the peak detector are below the label at high ratios; the manual notes this and the RMS detector gives textbook static curves.

## Amendments (2026-11-02, M2 review fix pass)

- Feedback topology realises the labelled ratio: the detector sees the compressed, pre-makeup signal, and the static reduction is `(R − 1)·(y − T)` with the knee compressed to `k / R` in the output domain, so the steady-state input/output slope is 1/R like the feed-forward path (measured 2/4/8:1 within 0.3). The one-sample loop through the attack one-pole is stable only while `(1 − a)·R < 1`, so the realisable ratio is capped at `1 / (1 − a)` (about 22:1 at a 1 ms attack, 2.7:1 at 0.1 ms, at 48 kHz); the editor's footer shows the effective ratio whenever it is below the label, which is the disclosure this ADR promised.
- Mix and the total makeup (manual, or auto capped at +24 dB) ramp per sample (mix from the smoothed parameter, makeup through its own 20 ms smoother, since the auto term is not a parameter).
