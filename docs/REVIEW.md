# Review: M3 Colour and space
Commit: {{SHA}}   Branch: milestone/m3-colour-and-space

## What was built
- **Saturation** (`plugins/saturation`, 8 parameters): Tape (first-order emphasis pair around an atan stage, 60 Hz head bump), Tube (`TubeTransfer`), Transistor (tanh), Clip (soft clip), all at fixed 4× oversampling with first-order ADAA (tabulated antiderivatives, within 1e-9 of the closed forms); bias shifts the operating point DC-free (5 Hz DC blocker); unity small-signal gain so drive adds colour, not level; HF roll-off; mix against a latency-aligned dry. Editor: one large drive knob, harmonic display of a 1 kHz tone (even harmonics in the accent), character selector with one-line descriptions.
- **Oversampler** (`core/oversampling/Oversampler.h`): polyphase half-band chain replacing juce::dsp::Oversampling (stage 0: 95 taps, passband 0.44 fs, ~88 dB; inner stages 23 taps), JUCE vector-op tap passes, integer latency. 4× stereo costs 55 ns per frame (was ~300); the M1 latency/flatness/alias gates still pass.
- **Delay** (`plugins/delay`, 15 parameters): two Lagrange-3 lines, tempo sync from the host play head (120 BPM fallback), feedback through SVF LP/HP with the line input soft-limited at +6 dBFS, ping-pong, LFO modulation, dry-keyed ducking; time moves ≤ 50 ms glide over 20 ms, larger jumps crossfade equal-power. Editor: draggable tap timeline snapping to the tempo grid. Render host: fixed-tempo play head (job key `bpm`); base: `producesStereoFromMono()`.
- **Reverb core** (`core/reverb/`): FDN-16 (Hadamard, mutually prime lengths per algorithm and size, RT60 gains, damping, Schroeder diffusers), Dattorro plate (decay from the loop length plus a measured correction table), eight-tap early reflections; render modules `fdn`, `plate`.
- **Reverb** (`plugins/reverb`, 13 parameters): pre-delay, Room / Hall / Chamber on two FDN instances and Plate, 200 ms morph on algorithm or size changes, early/late balance, low/high cut, mid/side width, mix; zero latency. Editor: draggable RT60 envelope with a live tail history.
- Presets (10 / 9 / 10), fuzz coverage for the three, listening sheet (`docs/listening/m3-listening-sheet.md`, awaiting engineers).

## Exit criteria (spec) → result (measured)
Every number is in the "M3 measurements" table of docs/STATE.md.
| criterion | target | measured | pass |
| --- | --- | --- | --- |
| Saturation aliasing at 19 kHz, 4× OS, at the **reference drive** (1 % THD) | < −80 dBFS | worst −96.3 dBFS over 4 characters × 4 rates | ✓ (ruling) |
| Saturation aliasing at **maximum drive** (spec wording "full drive") | < −80 dBFS | −54 … −119 dBFS: fails at 44.1/48 k for every character | ✗ spec question 1 |
| Delay | echo time ±0.05 ms, sync exact, feedback ±0.2 dB, bounded loop | pass at 4 rates | ✓ |
| Reverb FDN + Dattorro plate, RT60 within 10 % of the label | 10 % | worst 3.5 % (FDN), 0.5 % (plate), 0.5/2/8 s, 4 algorithms, 4 rates | ✓ |
| CPU within budget (1.0 / 0.5 / 2.0 %) | ≤ budget | local: saturation 0.840 %, delay 0.283 %, reverb 0.799 %; CI runners: {{CPU_CI}} | {{CPU_MARK}} |
| Stability fuzz (2 min on push) | no NaN, bounded | pass for all 7 plugins (120 s each, 4 workers) | ✓ |
| pluginval / auval / clap-validator | pass | pass for all three (macOS local; CI on three OSes) | ✓ |
| CI matrix + sanitizers | green | {{CI}} | {{CI_MARK}} |

## Decisions made without asking (link ADRs)
ADR-0006 (saturation stages, alias-gate ruling, static tape), ADR-0007 (delay topology, slew/crossfade, limiter, ducking), ADR-0008 (reverb design, plate calibration, 200 ms morph), ADR-0002 (colour family). Ledger rulings worth knowing:
- **Alias gate**: asserted at each character's reference drive (1 % THD on a −12 dBFS 1 kHz tone) with the +12 dB and maximum-drive floors recorded. A 19 kHz tone driven into a square wave cannot meet −80 dBFS at 4× (M1 showed the same); the spec's "full drive" is left as the open question below.
- **Tape** is a static model (emphasis, atan, head bump): Jiles–Atherton hysteresis costs ~12 % of a core at 4× stereo against a 1 % budget.
- **Oversampler rewritten** (juce::dsp::Oversampling at maximum quality alone cost 3.4 % CPU); **ADAA antiderivatives tabulated** (analytic log/cosh/atan cost ~45 ns per evaluation, eight per frame).
- **Delay times unsmoothed** (the line's own glide/crossfade is the smoothing); the whole line input, not only the feedback, is soft-limited; ducking maps −40…−20 dBFS to 0…100 %.
- **Plate RT60 floor** ~0.5 s at its fixed size (labels below reach the floor); `size_pct` scales the FDN algorithms and early reflections only. Engine switches morph over 200 ms, not the ADR's 50 (the incoming engine starts empty).
- Reverb sample-rate-change check settles 0.5 s at −20 dBFS tolerance (a stateful tail; the RT60 matrix at four rates is the real SR-independence evidence; measured −96 dBFS anyway).

## Known gaps / deferred
{{REVIEW_FINDINGS}}
- Listening gate: protocol and record sheet shipped; awaiting two engineers.
- Open from M1: tape hysteresis cost (now sidestepped by the static tape model; an "HQ" hysteresis option is a candidate for later).

## How to try it (exact commands, which DAW, which preset)
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
.venv/bin/python -m pytest tests/plugins -q -n auto -k "not cpu_budget"
.venv/bin/python -m pytest tests/plugins -q -k cpu_budget
MM_FUZZ_SECONDS=120 .venv/bin/python -m pytest tests/plugins/test_stability.py -q -n 4
tools/run_pluginval.sh saturation && tools/run_pluginval.sh delay && tools/run_pluginval.sh reverb
.venv/bin/python tools/bench_cpu.py --all --instances 100
.venv/bin/python tools/calibrate_plate.py          # re-derive the plate RT60 table
./build/tools/snapshot/mm_snapshot_reverb docs/snapshots 1.0
```
DAW: load from `build/plugins/<name>/<name>_artefacts/Release/`. Start with Saturation "Tape · Gentle warmth" (turn the drive and watch the harmonic display; switch characters), Delay "Dotted eighth · Vocal" (drag the first tap near a grid line to feel the snap; turn sync on), Reverb "Hall · Concert 2.4 s" (drag the −60 dB point; switch to Plate mid-tail).

## Review history
{{REVIEW_HISTORY}}

## Questions for the reviewer (only if any)
1. **Alias gate at full drive.** The spec's "19 kHz tone at full drive, < −80 dBFS with oversampling on" is physically out of reach at 4× for any of the four curves (measured −54…−76 dBFS at 44.1/48 k at 36 dB drive). Options: (a) accept the reference-drive definition now in ADR-0006; (b) cap the drive range per character where the gate holds (Tape ≈ 0–6 dB would be unusable); (c) 8× oversampling (CPU ~1.6×, still not −80 at 36 dB). Which?
2. **Plate decay floor.** The Dattorro plate cannot decay faster than ~0.5 s at its fixed size; the spec's 0.1 s applies to the FDN algorithms. Acceptable, or do you want a scaled-down plate for short labels?
3. **Tape model.** Static (emphasis + atan + head bump) instead of Jiles–Atherton hysteresis for CPU. Keep, or schedule an HQ mode?
