# ADR-0008: Reverb — FDN-16 for Room/Hall/Chamber, Dattorro plate, RT60 calibration

**Status:** Accepted (2026-11-02)

## Context

Spec: Reverb with algorithm (room / hall / plate / chamber), pre-delay, decay 0.1–20 s, size, damping, diffusion, low/high cut, early/late balance, mix; "feedback delay network (8–16 lines, Hadamard mixing) for hall/room; Dattorro plate for plate"; exit: "decay time measured within 10 % of labelled RT60"; budget < 2.0 %, latency 0.

## Decision

- **FDN-16** (`core/reverb/FDN.h`): 16 delay lines with a Hadamard feedback matrix (orthogonal, energy preserving, computed by the fast butterfly), lengths from a per-algorithm set in seconds (Room 11–43 ms, Hall 29–137 ms, Chamber 17–83 ms) scaled by `size_pct` and rounded to mutually prime sample counts; per-line gain `g_i = 10^(−3 L_i / (RT60 · fs))` so every line decays 60 dB in the labelled time; per-line one-pole damping (cutoff 20 kHz → 1 kHz over `damping_pct`); four Schroeder all-pass diffusers at the input (coefficient 0.5 + 0.25 · diffusion). Stereo: odd lines sum to L, even to R, with input decorrelation by feeding L and R into alternating lines.
- **Early reflections** (`core/reverb/EarlyReflections.h`): tapped delay with 8 taps per pattern (times scaled by size, gains by distance), mixed with the late tail by `early_late_pct`.
- **Plate** (`core/reverb/DattorroPlate.h`): Dattorro 1997 (input diffusers, two tanks with modulated all-passes). RT60 maps to the tank decay through the loop length: `decay = 10^(−3 · L_loop / (RT60 · fs))`, so the labelled time holds within the 10 % gate.
- **RT60 is calibrated at zero damping** (broadband); with damping the high band decays faster by design. The test measures Schroeder backward-integrated energy decay (fit −5 to −35 dB) at damping 0.
- Pre-delay through an `IntegerDelay` up to 250 ms. Low/high cut: SVF on the wet. Width: mid/side scaling of the wet (0 = mono, 200 = sides doubled).
- **Algorithm switch** crossfades the two engines' outputs over 50 ms; only the active engine runs once the fade is over (the other is reset).
- All buffers sized at `prepareDsp` for size 100 % and 250 ms pre-delay at the host rate; nothing allocates afterwards.

## Consequences

- RT60 within 10 % at 0.5/2/8 s for all four algorithms at four rates at damping 0; damped behaviour recorded.
- 16 lines × (delay read/write + one-pole + butterfly) ≈ 16 × 8 ns + 64 adds per frame ≈ 0.6–0.9 % per instance: within the 2 % budget with the plate's ~30 % extra.
- A 20 s decay at size 100 % needs ~5 s of total delay memory per instance (16 × 137 ms × 2 … well under 1 MB at 192 k).

## Amendments (2026-11-02, implementation)

- The plate's RT60 label is calibrated by measurement (`tools/calibrate_plate.py`, Schroeder T60 at 48 kHz, table in `DattorroPlate::correctedSeconds`) on top of the loop formula: the tank's onset (~0.36 s per half loop) makes short labels read long. The plate cannot decay faster than ~0.5 s at its fixed (Dattorro) size, so labels below 0.5 s reach that floor; `size_pct` scales the FDN algorithms and the early reflections only.
- The tank's modulated all-passes read with 4-point Lagrange interpolation: linear interpolation low-passed the broadband tank and shortened long decays by ~10 %.
- Algorithm and size changes crossfade between two FDN instances (or FDN and plate) over 200 ms, not 50: the incoming engine starts empty and needs ~0.3 s to fill, so a 50 ms fade dipped 10 dB.
- Measured: RT60 within 3.5 % (FDN) / 0.5 % (plate) of the label at 0.5 / 2 / 8 s and four sample rates.

## Amendments (2026-11-03, M3 review fix pass)

- **Size never restarts an engine.** The FDN's line lengths glide to their new mutually prime targets over 200 ms (fractional reads while moving) so a size change, or a size drag, keeps the tail; algorithm changes still crossfade between the two FDN instances (or FDN and plate), and a change requested while a fade is running is queued until the fade ends (resetting the outgoing engine cut the tail: measured −8 to −200 dB on a size drag). The plate ignores size.
- **Short decays.** Below RT60 0.6 s the four input diffusers shorten and soften in proportion (a 12.7 ms all-pass at g 0.675 rings for ~0.2 s on its own, which floored every FDN algorithm near 0.24 s). The plate's ~0.5 s floor stands as a reviewer question.
- **Unsmoothed parameters (hard rule 2 exceptions):** `size_pct` (an integer-length configuration; glided inside the FDN) and `predelay_ms` (an integer read position; changes crossfade between the old and new positions over 20 ms).
- **Mono output** carries the mid of the stereo wet (independent of width), so a mono bus never hears half the image.
