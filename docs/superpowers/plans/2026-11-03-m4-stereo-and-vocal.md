# M4 Stereo and Vocal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Two small compositions of existing modules on the shared base: a De-esser (band or split mode, lookahead, listen) and a Stereo Imager (three-band M/S width, pan, mono-below, correlation meter), each measured against the spec's mono-compatibility and correlation criteria and within its 0.4 % budget.

**Architecture:** `plugins/<name>/{params.json, Processor, Editor, presets/}` on `mm::PluginProcessorBase` / `mm::gui::PluginEditorBase`. De-esser = `SVF` band-pass detector (or `LinkwitzRiley` split) → `Detector` → `Lookahead` sliding max → threshold/range gain computer → `EnvelopeFollower` on the reduction → dynamic bell (`SVF` peak) or high-band gain. Imager = M/S encode → `LinkwitzRiley` LR4 three-band split of S with the same all-pass network on M → per-band side gain → decode → constant-power pan, `CorrelationMeter`. New shared GUI: `SibilanceDisplay` (waveform history with detected regions), `Goniometer` (Lissajous + correlation bar).

**Tech Stack:** as M0–M3. The render host's `lr_crossover` module (output lo + hi) supplies the all-passed reference for the imager's null test.

**Spec:** `docs/SPEC.md` (Mixing plugins rows De-esser / Stereo imager; Technical requirements: De-esser < 0.4 % / 0–3 ms, Imager < 0.4 % / 0; Testing: mono-compatibility and correlation; UI/UX rows Gate/de-esser and Imager/M/S; stereo accent violet), `CLAUDE.md` M4 exit criteria ("De-esser, Stereo Imager. Mono-compatibility and correlation tests."), roadmap `2026-11-01-suite-roadmap.md` M4.

## Global Constraints

- Budgets: De-esser < 0.4 %, Imager < 0.4 % (100 instances, 48 k / 64; CI limits scale by the runner factor). Latency: De-esser = lookahead 0–3 ms reported at runtime (unsmoothed, applied at block start with a 5 ms tap crossfade, like the gate: ADR-0009 exception); Imager 0.
- Mono compatibility (imager): L+R must not change with width (M is never scaled); width 0 collapses to mono (correlation 1.0); width 100 leaves the magnitude flat and the L/R relationship intact (an LR4 split is all-pass: ADR-0010); width 200 doubles the side (+6.02 dB).
- De-esser isolation: a 1 kHz tone is untouched (±0.05 dB) while sibilance at the detector frequency is reduced; `range_db` caps the reduction; `listen` outputs exactly dry − wet.
- Every continuous parameter smoothed (lookahead excepted); params.json single source; 4-rate matrix on the isolation and width tests; ADRs before code: ADR-0009 de-esser, ADR-0010 imager.

## Review Focus

1. De-esser on a full-band vocal-like signal (tone + noise burst at the detector frequency): the 1 kHz component stays within 0.05 dB in both modes while the band is reduced; no reduction when the band is below threshold. Tests: `test_deesser_only_band_reduced`, `test_deesser_no_reduction_below_threshold`.
2. De-esser lookahead changed 0 → 3 ms while audio runs: latency reported == measured at every block, no sample jump above 1.5× the input's own. Test: `test_deesser_lookahead_change_no_click`.
3. Imager width automated 0 → 200 on a stereo mix: L+R unchanged within 0.1 dB throughout; no sample jump above 2× the input's own. Tests: `test_imager_mono_sum_flat`, `test_imager_width_sweep_no_click`.
4. Mono input to the imager: nothing to widen (S = 0): output equals the all-passed input for every width; pan still moves it. Test: `test_imager_mono_input`.
5. Sample-rate change mid-stream (both): detector/crossover re-derived from Hz. Tests: `check_sr_change_mid_stream` per plugin.

---

### Task 1: ADR-0009 de-esser, ADR-0010 imager
**Files:** `docs/adr/0009-deesser-topology.md`, `docs/adr/0010-imager-topology.md`; roadmap ADR table renumbered (FIR 0011, limiter 0012, AI 0013, fine-tuning 0014).
- [ ] Write, commit `docs(adr): 0009 de-esser, 0010 imager; M4 plan`.

### Task 2: De-esser params and processor
**Files:** `plugins/deesser/{params.json, DeEsserProcessor.h/.cpp, DeEsserEditor (placeholder), CMakeLists.txt, presets/Default.mmpreset}`, `tests/plugins/test_deesser.py`, `tests/cpp/test_deesser.cpp`; `plugins/CMakeLists.txt`, `tests/cpp/CMakeLists.txt`, `tools/budgets.json` (deesser 0.4 present).
**Interfaces:** params `freq_hz` 2000–12000 (skew 0.5, default 6000), `threshold_db` −60…0 (default −24), `range_db` 0–24 (default 12), `mode` {Band, Split} (default Band), `width_oct` 0.2–2 (default 1.0), `listen` bool (ai false), `lookahead_ms` 0–3 (smoothing 0, default 1). `DeEsserProcessor`: `dspLatencySamples()` = lookahead; `maxDspLatencySamples()` = 3 ms; `float currentReductionDb()`; `bool sibilantNow()` atomic for the editor. Band mode: detector = SVF band-pass at freq with Q = 1 / (2 sinh(ln 2 / 2 · width)); gain = SVF peak at freq/Q with gain −reduction. Split mode: LR4 at freq; detector on the high band; high band × 10^(−reduction/20). Reduction = min(range, max(0, levelDb − threshold)) through an `EnvelopeFollower` (attack 0.5 ms, release 60 ms); linked (max over channels). Listen = dry − wet.
- [ ] **Step 1 (RED):** `test_deesser_only_band_reduced` (both modes, 4 rates: 1 kHz at −12 dBFS + 6 kHz at −6 dBFS: 1 kHz within ±0.05 dB of the input; 6 kHz reduced by ≥ 6 dB), `test_deesser_no_reduction_below_threshold` (6 kHz at −40: null < −100 dBFS Band, magnitude flat ±0.02 dB Split), `test_deesser_range_cap` (6 kHz 24 dB over threshold, range 6: reduction 6 ± 0.5), `test_deesser_split_sum_flat` (threshold 0: magnitude flat ±0.02 dB 20 Hz–20 kHz), `test_deesser_listen_is_difference` (listen == dry − wet within −100 dBFS), `test_deesser_lookahead_latency` (reported == measured, 0/1/3 ms, 4 rates), `test_deesser_lookahead_change_no_click`, base checks, `test_cpu_budget`. C++: `kNumParams == 9`, latency at 48 k with 3 ms == 144, `sibilantNow()` after a 6 kHz burst.
- [ ] **Steps 2–5:** implement → GREEN → commit `feat(deesser): band and split de-esser with lookahead and listen`.

### Task 3: De-esser editor
**Files:** `gui/SibilanceDisplay.h/.cpp` (scrolling waveform peak history with sibilant frames shaded in the accent; `push(const MeterFrame&, bool sibilant, float reductionDb)`), `plugins/deesser/DeEsserEditor.h/.cpp` (display, strip: freq, threshold, range, width, mode selector, lookahead; listen toggle prominent; GR meter), `tests/cpp/test_gui_deesser.cpp`, snapshot target.
- [ ] RED (every parameter has a control; `SibilanceDisplay` present; listen toggle; width 920) → implement (accent `accents::dynamics`? No: vocal tool → `accents::stereo` violet per the spec's family table? The de-esser is a vocal/dynamics tool: use `accents::dynamics`) → snapshots → commit `feat(deesser): editor with sibilance history and listen`.

### Task 4: Imager params and processor
**Files:** `plugins/imager/{params.json, ImagerProcessor.h/.cpp, ImagerEditor (placeholder), CMakeLists.txt, presets/Default.mmpreset}`, `tests/plugins/test_imager.py`, `tests/cpp/test_imager.cpp`; CMake lists.
**Interfaces:** params `xover_low_hz` 40–500 (skew 0.3, default 150), `xover_high_hz` 1000–8000 (skew 0.3, default 3000), `width_low_pct`, `width_mid_pct`, `width_high_pct` 0–200 (default 100), `pan` −100…100 (default 0), `mono_below_hz` 0–500 (default 0 = off, skew 0.5), `output_db` −12…12 (default 0). `ImagerProcessor`: `float currentCorrelation()`; a stereo sample ring for the goniometer (`int pullScope(float* l, float* r, int max)`). M/S encode (M = (L+R)/2, S = (L−R)/2); S → LR4 at xover_low and xover_high (bands low/mid/high) → × width_b/100 → sum; M → the same two LR4 networks summed (all-pass, so M and S stay phase-aligned); mono-below: S low band below `mono_below_hz` removed (a third LR4 on S when on); decode; constant-power pan; output gain.
- [ ] **Step 1 (RED):** `test_imager_width_0_is_mono` (correlation 1.0 ± 0.001 over a stereo noise pair), `test_imager_width_100_flat_and_coherent` (magnitude flat ±0.02 dB; null < −100 dBFS against the `lr_crossover` all-passed reference, 4 rates), `test_imager_width_200_side_gain` (side +6.02 ± 0.1 dB in each band), `test_imager_mono_below` (side energy below f < −60 dB relative; above unchanged), `test_imager_mono_sum_flat` (L+R unchanged ±0.1 dB for widths 0/50/150/200, every band), `test_imager_pan` (−100: R = 0; +100: L = 0; 0: equal; constant power at ±50), `test_imager_mono_input`, `test_imager_width_sweep_no_click`, `test_imager_correlation_meter_accuracy` (C++: known-correlation pairs ±0.02), base checks, `test_cpu_budget`. C++: `kNumParams == 10`, latency 0.
- [ ] **Steps 2–5:** implement → GREEN → commit `feat(imager): three-band M/S width with mono-below, pan and correlation meter`.

### Task 5: Imager editor
**Files:** `gui/Goniometer.h/.cpp` (Lissajous of the scope ring rotated 45° so mono is vertical, width drawn as the spread, correlation bar −1…+1 under it; `setCorrelation`, `pushScope`), `plugins/imager/ImagerEditor.h/.cpp` (goniometer main display; strip: three width knobs with crossover knobs between them, pan, output; drawer: mono-below with its frequency line drawn on a small `SpectrumDisplay` of the output), tests, snapshot. Accent `accents::stereo`.
- [ ] RED → implement → snapshots → commit `feat(imager): editor with goniometer and correlation bar`.

### Task 6: Presets, fuzz, STATE, REVIEW, review, CI, tag
- [ ] ≥ 8 presets each (De-esser "Vocal · Gentle", "Vocal · Firm 6 kHz", "Hi-hat · Tame", …; Imager "Mix · Wide top", "Bass · Mono below 120", "Narrow · Vocal", …), fuzz entries (`MAX_STATIC_GAIN_DB` deesser 12, imager 6 + 12 + 12; pure-gain bases), STATE M4 measurements, archive the M3 review, REVIEW.md, fresh whole-branch review + fix pass, push, CI green, tag `review/m4-stereo-and-vocal`, merge, publish. Print `READY FOR REVIEW: M4 Stereo and vocal`.
