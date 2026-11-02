# M3 Colour and Space Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Three plugins on the shared base: Saturation (four characters, fixed 4× oversampling with ADAA), Delay (dual fractional line, tempo sync, ping-pong, feedback filter, modulation, ducking) and Reverb (16-line Hadamard FDN for Room/Hall/Chamber, Dattorro plate for Plate), each measured against the spec's exit criteria and within budget, with new core modules for the reverb.

**Architecture:** `plugins/<name>/{params.json, Processor, Editor, presets/}` on `mm::PluginProcessorBase` / `mm::gui::PluginEditorBase`. Saturation composes `Oversampler(4×)` + `ADAA1<shaper>` + `TubeTransfer` + pre/post one-pole EQ. Delay composes two `FractionalDelayLine` (Lagrange-3, slewed time), `SVF` feedback LP/HP, an LFO, `TempoSync`, and an `EnvelopeFollower` on the dry input for ducking. Reverb adds `core/reverb/{FDN.h, DattorroPlate.h, EarlyReflections.h}`. New shared GUI: `HarmonicDisplay`, `TapDisplay`, `DecayDisplay`.

**Tech Stack:** as M0–M2. Render host: new core modules `fdn`, `plate` for the reverb measurements; the saturation and delay are measured through the hosted VST3 as in M2.

**Spec:** `docs/SPEC.md` (Mixing plugins rows Saturation / Delay / Reverb; Technical requirements table: Saturation < 1.0 % / ~1 ms / 4× fixed, Delay < 0.5 % / 0, Reverb < 2.0 % / 0; Testing and validation: Aliasing row; UI/UX rows Saturation and Delay/reverb; accent teal for time/space), `CLAUDE.md` M3 exit criteria, roadmap `2026-11-01-suite-roadmap.md` M3 task list, M1 review (`docs/reviews/m1-core-dsp.md`) open questions on the alias gate and tape cost.

## Global Constraints

- Exit criteria (CLAUDE.md M3): "Saturation (aliasing < −80 dBFS at 19 kHz, 4× OS), Delay, Reverb (FDN + Dattorro plate). Reverb decay time measured within 10% of labelled RT60." Budgets: Saturation < 1.0 %, Delay < 0.5 %, Reverb < 2.0 % (100 instances, 48 k / 64). Latency: Saturation ~1 ms (4× oversampler, reported), Delay 0, Reverb 0.
- Alias gate ruling (ADR-0006, spec question kept open from M1): the −80 dBFS gate is asserted at 4× with the **reference drive** of each character (the drive at which a 1 kHz tone at −12 dBFS produces 1 % THD) at 44.1/48/96/192 k; the alias floor at drive +12 dB and at maximum drive is **recorded** per character and rate, not gated. A 19 kHz tone driven into a square wave cannot meet −80 dBFS at 4× by any means short of ~32× oversampling.
- Every nonlinear stage runs inside the 4× oversampler; every continuous parameter smoothed; params.json is the single source; sample-rate matrix 44.1/48/96/192 k on every measurement.
- ADRs before code: ADR-0006 saturation stages, ADR-0007 delay topology, ADR-0008 reverb design.
- No allocation on the audio thread: reverb delay lines and the oversampler are sized in `prepareDsp` for the maximum size/pre-delay; host-rate independence by scaling every delay length in samples from the design in seconds.

## Review Focus

1. Saturation at maximum drive on a full-scale square-ish input (bass with harmonics) → bounded output (≤ +6 dBFS before the output trim), no DC build-up beyond −60 dBFS from the bias control at any setting. Test: `test_sat_bounded_and_dc`.
2. Delay time dragged from 1 ms to 4000 ms while audio runs → the slew keeps every sample-to-sample jump within 2× the input's own; feedback 120 % never grows past the soft limit (+6 dBFS). Tests: `test_delay_no_zipper_on_time_change`, `test_delay_feedback_soft_limit`.
3. Reverb at decay 20 s with size and damping moved live → energy decreases monotonically once the input stops (no limit cycles, no blow-up), and switching algorithm mid-tail fades rather than clicks. Tests: `test_reverb_stability_long_decay`, `test_reverb_algorithm_switch_click`.
4. Sample-rate change mid-stream on all three → delay times, RT60 and the oversampler latency re-derive from seconds, not samples. Tests: `check_sr_change_mid_stream` per plugin with a settle, plus `test_delay_time_accuracy` / `test_reverb_rt60` across the four rates.
5. Mono-in / stereo-out and stereo-in configurations: ping-pong and reverb width behave (mono input produces a decorrelated stereo tail; mono output sums without level jump). Tests: `check_channel_configs` plus `test_reverb_mono_input`, `test_delay_ping_pong_alternation`.

---

### Task 1: ADR-0006 saturation stages, ADR-0007 delay topology, ADR-0008 reverb design

**Files:** Create `docs/adr/0006-saturation-stages.md`, `docs/adr/0007-delay-topology.md`, `docs/adr/0008-reverb-design.md`.

- [ ] One page each: status, context (spec rows, M1 numbers), decision, consequences. ADR-0006 settles the alias-gate ruling and the tape model (static, not Jiles–Atherton: 311 ns/sample × 4× × 2 channels ≈ 12 % of a core against a 1 % budget). ADR-0007 settles time-change slew (20 ms log-time slew through `FractionalDelayLine::setSlewMs`), feedback soft limit (tanh at +6 dBFS in the loop), ducking (dry-keyed `EnvelopeFollower`, attack 5 ms, release `duck_release_ms`). ADR-0008 settles FDN-16 (Hadamard, mutually prime lengths scaled by size, per-line one-pole damping, four input all-pass diffusers), RT60 → per-line gain `g_i = 10^(−3 L_i / (RT60 fs))`, early reflections (tapped delay, 8 taps per algorithm), plate = Dattorro 1997 with the tank decay set from the label through the measured loop length, algorithm switch = 50 ms crossfade between two engines' outputs (only the active engine runs after the fade).
- [ ] Commit `docs(adr): 0006 saturation stages, 0007 delay topology, 0008 reverb design`.

### Task 2: Saturation params and processor

**Files:** Create `plugins/saturation/{params.json, SaturationProcessor.h/.cpp, SaturationEditor.h/.cpp (GenericAudioProcessorEditor placeholder), CMakeLists.txt, presets/Default.mmpreset}`, `tests/plugins/test_saturation.py`, `tests/cpp/test_saturation.cpp`; Modify `plugins/CMakeLists.txt`, `tests/cpp/CMakeLists.txt`, `tools/budgets.json` (saturation 1.0 already present).

**Interfaces:**
- params: `drive_db` 0–36 (skew 0.5, default 6, smoothing 20), `character` {Tape, Tube, Transistor, Clip} (default Tape), `bias` −1..1 (default 0, smoothing 20), `hf_rolloff_hz` 2000–20000 (skew 0.3, default 20000 = off, smoothing 20), `mix_pct` 0–100 (default 100), `output_db` −24..+12 (default 0), `oversample_display` none. Reserved `bypass`, `trim_db`.
- Chain per channel (inside the 4× block): input gain `10^(drive/20)` → character stage → bias (even-order: `y += bias * 0.5 * (x*x − mean)` realised as a DC-blocked asymmetric term: `shaper(x + 0.3*bias)` − `shaper(0.3*bias)`) → `OnePoleLP` HF roll-off at `hf_rolloff_hz` (bypassed at 20 kHz) → output gain `10^(output/20) / drive compensation` (unity small-signal gain: divide by the shaper's slope at 0, so drive adds colour, not level) → mix with the dry (dry delayed by the oversampler latency through the base's dry path: the processor mixes inside using its own `IntegerDelay` on the dry copy).
- Characters: Tape = `ADAA1<atanS, atanAD1D>` with pre-emphasis/de-emphasis one-pole pair (+3 dB at 10 kHz before, inverse after) and a +2 dB low shelf at 60 Hz (head bump) on the output; Tube = `TubeTransfer::set(drive, bias)` (static, through ADAA by its own antiderivative is unavailable → run at 4× without ADAA, record alias); Transistor = `ADAA1<tanh, tanhAD1D>` asymmetric by bias; Clip = `ADAA1<softClip knee 0.5, softClipAD1D>`.
- `int dspLatencySamples()` = oversampler latency (4×); `maxDspLatencySamples()` same (fixed factor).
- `float referenceDriveDb(int character)` static: the drive giving 1 % THD on a −12 dBFS 1 kHz sine (computed once at prepare by bisection on the static curve; exposed for the tests).
- `std::array<float, 16> harmonicLevelsDb(float driveDb, int character, float bias)` (GUI thread): runs a 1 kHz sine at −12 dBFS through a private copy of the static stage at 4×, FFT-free Goertzel at harmonics 1–16.

- [ ] **Step 1: Failing tests.** `test_sat_aliasing_19k` (per character, 4 rates: 19 kHz tone at −12 dBFS at the reference drive → every bin below 19 kHz (excluding the fundamental ±50 Hz) < −80 dBFS; also records the floor at +12 dB and at 36 dB), `test_sat_harmonic_balance` (1 kHz at −12 dBFS, drive 12: even/odd harmonic energy ratio strictly increases with bias from −1 to 1 through 0 where the even energy is < −60 dB for Tube/Transistor/Clip (Tape has its head-bump asymmetry recorded)), `test_sat_reference_drive_thd` (THD at the reference drive within 1.0 % ± 0.2 %), `test_sat_latency_reported` (== measured by cross-correlation, == oversampler latency at 4×, 4 rates), `test_sat_null_mix_zero` (mix 0: residual vs delayed input < −90 dBFS), `test_sat_unity_small_signal` (drive 0..36 on a −40 dBFS tone: level change within ±0.5 dB: drive compensation), `test_sat_bounded_and_dc` (Review Focus 1), base checks (variable blocks, SR change settle 0.1 s, state round trip, extremes, denormal tail, channel configs), `test_cpu_budget` (< 1.0 %). C++: `kNumParams == 8`, latency at 48 k > 0 and equals `oversampler.latencySamples()`, `referenceDriveDb` for each character within 0–24 dB, `harmonicLevelsDb` returns finite values with the fundamental at ≈ −12 dB.
- [ ] **Step 2–5:** RED → implement per ADR-0006 → GREEN → commit `feat(saturation): four characters at fixed 4x with ADAA, drive compensation, HF roll-off, mix`.

### Task 3: Saturation editor

**Files:** Create `gui/HarmonicDisplay.h/.cpp` (bars for harmonics 1–16, even bars in the accent, odd in the text colour, dB scale −90..0; `setLevels(const std::array<float,16>&)`), `plugins/saturation/SaturationEditor.h/.cpp`; Modify `tools/snapshot/CMakeLists.txt` (`mm_add_snapshot(saturation …)`), `tests/cpp/CMakeLists.txt`.

- [ ] **Step 1: Failing GUI test** `tests/cpp/test_gui_saturation.cpp`: every non-header parameter has a control; a `HarmonicDisplay` exists; the character selector shows a one-line description label that changes with the choice; a large drive knob (bounds ≥ 160 px) is present; width 920.
- [ ] **Step 2–5:** implement (board: big drive knob left, harmonic display as the main display, character tabs with descriptions "Tape: soft, warm, head bump" / "Tube: even-order glow" / "Transistor: hard odd-order edge" / "Clip: flat-top limiting colour", strip: bias, HF roll-off, mix, output; accent `accents::timeSpace` for the family? — no: Saturation belongs to the colour family: use `accents::dynamics`? The spec lists accents per family: EQ blue, dynamics amber, time/space teal, stereo violet, mastering white/gold; saturation is unlisted → use amber-orange `juce::Colour(0xffe08a5c)` added as `accents::colour`), snapshots both themes, commit `feat(saturation): editor with harmonic display and character tabs`.

### Task 4: Delay params and processor

**Files:** Create `plugins/delay/{params.json, DelayProcessor.h/.cpp, DelayEditor (placeholder), CMakeLists.txt, presets/Default.mmpreset}`, `tests/plugins/test_delay.py`, `tests/cpp/test_delay.cpp`; Modify `plugins/CMakeLists.txt`, `tests/cpp/CMakeLists.txt`, `tools/render/Main.cpp` (job key `bpm`: the host exposes a `juce::AudioPlayHead` with that tempo to the plugin; default 120).

**Interfaces:**
- params: `time_l_ms` 1–4000 (skew 0.3, default 375, smoothing 0 — the line slews itself, ADR-0007 exception recorded), `time_r_ms` (same, default 500), `link_lr` bool (default false: when on, R follows L), `sync` {Off, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4 dotted, 1/8 dotted, 1/16 dotted, 1/4 triplet, 1/8 triplet, 1/16 triplet} (default Off), `feedback_pct` 0–120 (default 35, smoothing 20), `ping_pong` bool, `fb_lp_hz` 200–20000 (skew 0.3, default 8000), `fb_hp_hz` 20–2000 (skew 0.3, default 100), `mod_depth_ms` 0–10 (default 0), `mod_rate_hz` 0.05–5 (skew 0.4, default 0.5), `mix_pct` 0–100 (default 35), `duck_amount_db` 0–24 (default 0), `duck_release_ms` 50–2000 (skew 0.3, default 300).
- `FractionalDelayLine<float>` prepared for 4.5 s at the host rate, 2 channels, Lagrange-3, slew 20 ms; time in samples = `ms * sr / 1000` (sync: `mm::noteMs(bpm, division, dotted, triplet)`), BPM from `getPlayHead()->getPosition()->getBpm()` at block start, else 120.
- Feedback: `fb = wet_out * feedback` → SVF LP then HP → soft limit `tanh(fb / 2) * 2` (ceiling +6 dBFS) → into the line (ping-pong: L feeds R's input and R feeds L's). Modulation: sine LFO per channel (R at 90°) adds `depth * sin` ms to the read position. Ducking: `EnvelopeFollower` on the dry peak (attack 5 ms, release `duck_release_ms`); wet gain = `10^(−duck_amount * env_norm / 20)` with `env_norm` = envelope above −40 dBFS mapped 0..1.
- `tools/render` job key `bpm` (double, default 120).

- [ ] **Step 1: Failing tests.** `test_delay_time_accuracy` (impulse, first echo at the labelled time ± 0.05 ms, 4 rates, times 1 / 37.5 / 375 / 4000 ms), `test_delay_tempo_sync` (120 BPM: 1/8 = 250 ms, 1/4 dotted = 750 ms, 1/8 triplet = 166.67 ms; job bpm 90: 1/4 = 666.67 ms), `test_delay_feedback_decay` (tap n amplitude = fb^n within ±0.2 dB for fb 0.5 over 6 taps with the filters open), `test_delay_feedback_soft_limit` (fb 120 % on a burst: peak never exceeds +6 dBFS, output finite over 10 s), `test_delay_ping_pong_alternation` (mono impulse: taps alternate L/R), `test_delay_filter_in_loop` (fb LP 1 kHz: the 5th tap of a 4 kHz tone is ≥ 20 dB below the 5th tap of a 200 Hz tone), `test_delay_modulation` (depth 5 ms, rate 1 Hz: the echo time of a repeated click varies by ±5 ms ± 10 %), `test_delay_ducking` (loud dry → wet level drops by duck_amount ± 1 dB; silence → recovers within 3 × release), `test_delay_no_zipper_on_time_change` (Review Focus 2), base checks (SR change settle 0.1 s), `test_cpu_budget` (< 0.5 %). C++: `kNumParams == 15`, latency 0, 2 output channels on mono input.
- [ ] **Step 2–5:** RED → implement → GREEN → commit `feat(delay): dual fractional delay with sync, ping-pong, filtered feedback, modulation, ducking`.

### Task 5: Delay editor

**Files:** Create `gui/TapDisplay.h/.cpp` (timeline 0–4 s log-ish; L taps above, R taps below the axis, bars decaying by the feedback; drag the first tap to set the time; when `sync` is off and the drag lands within 5 % of a note value of the current BPM the time snaps and a note label shows; `std::function<void(int channel, double ms)> onTimeChange`, `onGesture`), `plugins/delay/DelayEditor.h/.cpp`; Modify snapshot and test CMake.

- [ ] **Step 1: Failing GUI test** `tests/cpp/test_gui_delay.cpp`: controls for every parameter; `TapDisplay` present; `dragTap(0, msFor(250))` sets `time_l_ms` to 250 ± 2 % and `snappedLabel()` == "1/8" at 120 BPM; width 920.
- [ ] **Step 2–5:** implement (accent `accents::timeSpace`), snapshots, commit `feat(delay): editor with draggable tap display and tempo grid`.

### Task 6: Reverb core modules

**Files:** Create `core/reverb/FDN.h` (`template <typename T, int N = 16> class FDN`: `prepare(sr, maxSizeSeconds)`, `setLengthsSeconds(const std::array<double,N>&)`, `setRT60(seconds)`, `setDamping(0..1)` (per-line one-pole LP cutoff 20 kHz → 1 kHz), `setDiffusion(0..1)` (four input `FirstOrderAllPass`… as Schroeder all-passes with coefficients 0.5–0.75), `processStereo(inL, inR, outL, outR)`), `core/reverb/DattorroPlate.h` (`prepare(sr)`, `setDecay(0..1)`, `setDamping`, `setDiffusion`, `setPredelayMs`, `setRT60(seconds)` which maps through the tank loop length `L_loop` with `decay = 10^(−3 L_loop/(RT60 fs))`, `processStereo`), `core/reverb/EarlyReflections.h` (8 taps per pattern {Room, Hall, Chamber}, `setSize`, `setPattern`, `processStereo`); `tests/cpp/test_fdn.cpp`, `tests/cpp/test_plate.cpp`, `tests/core/test_reverb_modules.py`; Modify `tools/render/Main.cpp` (modules `fdn`, `plate` with params rt60_s, size_pct, damping_pct, diffusion_pct).

- [ ] **Step 1: Failing tests.** C++: Hadamard mixing is orthogonal (energy preserving: output energy == input energy for the 16×16 matrix); FDN lengths are mutually prime after scaling (gcd test); RT60 test in Python: impulse → Schroeder backward integration → T60 (fit −5..−35 dB) within 10 % of 0.5/2/8 s for `fdn` and `plate` at 4 rates with damping 0; damping 100 %: T60 at 8 kHz band at least 2× shorter than at 500 Hz (recorded); plate energy monotonic decrease.
- [ ] **Step 2–5:** RED → implement → GREEN → commit `feat(core): FDN-16, Dattorro plate, early reflections with RT60 calibration`.

### Task 7: Reverb params and processor

**Files:** Create `plugins/reverb/{params.json, ReverbProcessor.h/.cpp, ReverbEditor (placeholder), CMakeLists.txt, presets/Default.mmpreset}`, `tests/plugins/test_reverb.py`, `tests/cpp/test_reverb.cpp`; Modify `plugins/CMakeLists.txt`, `tests/cpp/CMakeLists.txt`.

**Interfaces:** params: `algorithm` {Room, Hall, Plate, Chamber} (default Hall), `predelay_ms` 0–250 (default 20), `decay_s` 0.1–20 (skew 0.3, default 2.0), `size_pct` 10–100 (default 60), `damping_pct` 0–100 (default 40), `diffusion_pct` 0–100 (default 70), `low_cut_hz` 20–1000 (skew 0.3, default 80), `high_cut_hz` 1000–20000 (skew 0.3, default 12000), `early_late_pct` 0–100 (default 40 = early share), `mix_pct` 0–100 (default 30), `width_pct` 0–200 (default 100). Engines: FDN (Room/Hall/Chamber length sets and ER patterns) and Plate; algorithm switch fades 50 ms (both engines run during the fade only). Pre-delay via `IntegerDelay`. Low/high cut: SVF HP/LP on the wet. Width: mid/side scaling of the wet.

- [ ] **Step 1: Failing tests.** `test_reverb_rt60` (4 algorithms × 0.5/2/8 s × 4 rates, damping 0, size 60: Schroeder T60 within 10 %), `test_reverb_predelay` (first wet energy arrives at predelay ± 1 ms), `test_reverb_stability_long_decay` (decay 20 s, size 100, damping 0, diffusion 100: after the input stops, 1 s-window energy strictly decreases for 20 s; finite), `test_reverb_mono_input` (mono in → stereo tail with |correlation| < 0.5 after 200 ms; width 0 → identical channels), `test_reverb_algorithm_switch_click` (Review Focus 3: 1 ms envelope step < 3 dB during the fade), `test_reverb_null_mix_zero` (< −120 dBFS), base checks (SR change settle 1 s + tolerance −40 dBFS because the tail is stateful: recorded), `test_cpu_budget` (< 2.0 %). C++: `kNumParams == 13`, latency 0.
- [ ] **Step 2–5:** RED → implement → GREEN → commit `feat(reverb): FDN room/hall/chamber and Dattorro plate with early reflections, pre-delay, width`.

### Task 8: Reverb editor

**Files:** Create `gui/DecayDisplay.h/.cpp` (time axis 0–max(2·decay, 1 s); analytic envelope `exp(−6.91 t / RT60)` in dB with the early/late split shaded; drag the envelope's −60 dB point to set the decay; `onDecayChange(seconds)`, `onGesture`; a live tail meter from the wet RMS history), `plugins/reverb/ReverbEditor.h/.cpp`; Modify snapshot and test CMake.

- [ ] **Step 1: Failing GUI test** `tests/cpp/test_gui_reverb.cpp`: every parameter has a control; `DecayDisplay` present; `dragDecayTo(xFor(4.0 s))` sets `decay_s` to 4.0 ± 5 %; algorithm tabs; width 920.
- [ ] **Step 2–5:** implement (accent teal), snapshots, commit `feat(reverb): editor with draggable decay display`.

### Task 9: Presets, fuzz, listening sheet

**Files:** `plugins/{saturation,delay,reverb}/presets/*.mmpreset` (≥ 8 each: Saturation "Tape · Gentle warmth", "Tube · Vocal glow", "Transistor · Drum bus edge", "Clip · Safety", …; Delay "Slapback · 1/16", "Dotted eighth · Vocal", "Ping-pong · 1/4", "Dub · Filtered feedback", …; Reverb "Hall · Concert 2.4 s", "Room · Drum 0.6 s", "Plate · Vocal 1.8 s", "Chamber · Strings 3 s", …), `tests/plugins/test_stability.py` (MAX_STATIC_GAIN_DB entries: saturation 36 + 12 + 12, delay 12 + 6 (soft limit), reverb 12), `docs/listening/m3-listening-sheet.md` (blind A/B template per the spec's listening gate, marked awaiting engineers); C++ preset checks in each plugin test.

- [ ] Steps: fuzz passes on the three (120 s), presets load (≥ 8), commit `test(plugins): M3 presets, fuzz coverage, listening sheet`.

### Task 10: STATE, REVIEW, tag

- [ ] Fill `docs/STATE.md` M3 measurements (alias floors per character/rate at reference, +12 and max drive; delay time errors; RT60 errors per algorithm/rate; CPU per plugin per OS; pluginval), archive the M2 review to `docs/reviews/m2-channel-essentials.md`, write `docs/REVIEW.md` (the alias gate ruling is the first reviewer question with the full table), fresh whole-branch review + fix pass, push, CI green, tag `review/m3-colour-and-space`, merge to main, publish the review page. Print `READY FOR REVIEW: M3 Colour and space`.
