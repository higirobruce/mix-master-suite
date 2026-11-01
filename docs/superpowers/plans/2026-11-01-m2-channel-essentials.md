# M2 Channel Essentials Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Three plugins that exercise filters, dynamics and sidechain on the shared base: Channel EQ, Channel Compressor and Gate, each measured against the spec's exit criteria and within its CPU budget, with the shared stability fuzz applied to every plugin so far.

**Architecture:** Each plugin is `plugins/<name>/{params.json, Processor, Editor, presets/}` on `mm::PluginProcessorBase` and `mm::gui::PluginEditorBase`, composing M1 modules: Channel EQ = 7 bands of matched/RBJ biquads in double (optional 2× `Oversampler`); Channel Compressor = `Detector → SidechainFilter → GainComputer → EnvelopeFollower` on the gain (feed-forward) with an optional feedback topology, sidechain bus, mix; Gate = the same detector chain with an expander gain computer, hysteresis, hold, and `Lookahead`. New shared GUI pieces land in `gui/`: `CurveDisplay`, `SpectrumDisplay`, `TransferCurveDisplay`, `HistoryDisplay`, `AdvancedDrawer`.

**Tech Stack:** as M0/M1. New: `tests/plugins/test_stability.py` (parameter fuzz at audio rate, SR/buffer changes), nightly CI job.

**Spec:** `docs/SPEC.md` (Mixing plugins rows Gate / Channel EQ / Channel Compressor; Technical requirements table; Testing and validation; UI/UX EQs and Compressors rows), `CLAUDE.md` M2 exit criteria, roadmap `2026-11-01-suite-roadmap.md` M2 task list, M1 interfaces in `core/`.

## Global Constraints

- Exit criteria (CLAUDE.md M2): "Channel EQ, Channel Compressor, Gate. Frequency response within ±0.1 dB / ±2°; attack/release within 10%; ratio within 0.1; CPU within budget table." Spec phase 2 exit: "null tests against reference transfer curves within 0.1 dB."
- Budgets: Gate < 0.3 %, Channel EQ < 0.5 %, Channel compressor < 0.4 % per instance (100 instances, 48 k / 64). Latency: Gate 0–5 ms lookahead; EQ 0 (2× adds ~1 ms); compressor 0. Precision: EQ 64-bit filters; dynamics 32-bit. Sidechain: Gate yes, compressor yes, EQ no.
- Null test: EQ flat < −120 dBFS; dynamics at 1:1 < −90 dBFS. Stability: parameter fuzz at audio rate on noise, SR and buffer-size changes mid-stream, no NaN/denormal/click/crash over 1 hour (nightly) / 2 minutes (push).
- ADRs before code: ADR-0003 EQ bands and analog Q, ADR-0004 compressor topologies and program-dependent release (must settle the two M1 spec questions: ballistics on the gain, timing measured on steps), ADR-0005 gate hysteresis and lookahead.
- Every parameter id carries its unit suffix; every continuous parameter smoothed; the AI schema is generated (no hand edits).

## Review Focus

1. EQ band dragged across another band or to the frequency extremes while audio runs → no click, no instability (coefficients recomputed at block rate from smoothed values, double state). Test: `test_eq_band_sweep_stability`.
2. Compressor with ratio 1:1 and threshold at max → exact null (< −90 dBFS); with ∞:1 and threshold −60 → bounded, finite, no DC. Tests: `test_comp_null_unity`, `test_comp_extremes`.
3. Gate on a signal hovering at the threshold → hysteresis prevents chatter (≤ 1 transition per modulation cycle). Test: `test_gate_hysteresis_no_chatter`.
4. External sidechain bus disabled by the host while `ext_key` is on → falls back to the internal key silently (no silence, no crash). Test: `test_comp_ext_key_without_bus`.
5. Mono instance of the compressor/gate with stereo link → link is a no-op; mono→stereo EQ processes once and copies. Test: `test_channel_configs` per plugin (base check) plus `test_comp_mono_link`.

---

### Task 1: ADR-0003, ADR-0004, ADR-0005

**Files:** Create `docs/adr/0003-channel-eq-bands.md`, `docs/adr/0004-compressor-topology.md`, `docs/adr/0005-gate-hysteresis.md`.

- [ ] **Step 1:** ADR-0003: 7 bands (HP, LS, 3× bell, HS, LP); HP/LP slopes 6–48 dB/oct as cascaded sections (6 = one-pole, 12 = Butterworth biquad, 24 = LR-style two biquads Q 0.54/1.31, 36/48 = Butterworth cascades); bells and shelves = matched designs (`mm::matched`) in double; "analog Q" = proportional-Q (Q scales with |gain|: Q_eff = Q · (1 + |g|/12)) vs "digital" = constant Q; optional 2× oversampling via `Oversampler` with reported latency; analyser = `SpectrumAnalyser` pre/post; coefficient update per block from smoothed freq/gain/Q when any changed > 1e-4.
- [ ] **Step 2:** ADR-0004: feed-forward default (detector on input), feedback option (detector on the output of the previous sample, one-sample delay); ballistics on the gain reduction in the log domain (M1 finding); program-dependent "auto" release = M1 auto (60→500 ms by time above); knee quadratic; peak/RMS detector; stereo link % = blend of max and per-channel levels; mix = parallel blend post-makeup; auto makeup = +|threshold|·(1 − 1/ratio)/2; spec timing gates measured on steps (M1 ruling), tone-burst numbers recorded.
- [ ] **Step 3:** ADR-0005: expander/gate chain = detector → `GainComputer{expander}` with hysteresis implemented as two thresholds (open at T, close at T − hysteresis_db) driving a state machine {closed, opening, open, hold, closing}; hold timer restarts on retrigger; lookahead 0–5 ms via `Lookahead` (latency reported); range floor; listen-to-sidechain.
- [ ] **Step 4:** Commit `docs(adr): 0003 EQ bands, 0004 compressor, 0005 gate`.

### Task 2: Channel EQ params and processor

**Files:** Create `plugins/channel_eq/{params.json, ChannelEqProcessor.h/.cpp, CMakeLists.txt}`, `tests/plugins/test_channel_eq.py`, `tests/cpp/test_channel_eq.cpp`; Modify `plugins/CMakeLists.txt`, `tools/budgets.json` (already has channel_eq 0.5).

**Interfaces:**
- params: `hp_enabled` bool, `hp_freq_hz` 20–2000 (skew 0.3), `hp_slope` choice {6,12,18,24,36,48}; `lp_enabled`, `lp_freq_hz` 1000–20000, `lp_slope`; bands b1..b5: `b<n>_enabled`, `b<n>_type` choice {Bell, LowShelf, HighShelf} (b1 default LowShelf, b5 HighShelf), `b<n>_freq_hz` 20–20000 (skew 0.25), `b<n>_gain_db` −18..18, `b<n>_q` 0.1–10 (skew 0.3); `q_mode` {Digital, Analog}; `oversample` {Off, 2x}; `analyser` {Off, Pre, Post, Pre+Post} (`ai: false`); defaults per the mockup "Vocal · Presence lift" preset go into presets, not defaults.
- `ChannelEqProcessor : PluginProcessorBase` with `dspLatencySamples()` = oversampler latency; `std::array<mm::Biquad<double>, N> ` per channel; `const std::vector<BandState>& bandsForDisplay()` (freq/gain/q/type/enabled snapshot for the editor) and `std::complex<double> evaluateAt(double hz)` (product of all section responses, for the curve).

- [ ] **Step 1: Write failing tests.** Python: `test_eq_band_response` (each type × f ∈ {60, 250, 1000, 4000, 12000} × gain ∈ {−12, +6} × Q ∈ {0.7, 2.0} × 4 rates: render impulse, compare to the plugin's own `evaluateAt` exported through a new `mm_render --dump-response <plugin> <json params> <sr>` command? — simpler: compare to `designs.matched` / `designs.rbj` mirrors with the ADR-0003 slope compositions, ±0.1 dB / ±2° over 20 Hz–min(20 k, 0.45 fs)); `test_eq_null_flat` (all bands at 0 dB and filters off: < −120 dBFS, 4 rates); `test_eq_hp_lp_slopes` (−3 dB at fc ±0.1 dB; slope one octave away within ±1 dB of nominal); `test_eq_analog_q` (bell at +12 dB in Analog mode is narrower than Digital: −3 dB bandwidth ratio within 10 % of the ADR formula); `test_eq_oversample_latency` (reported == measured impulse delay; response within ±0.1 dB of the 1× response below 0.4 fs); `test_eq_band_sweep_stability` (b3 freq automation 20 → 20 k over 2 s on noise at audio rate: no NaN, peak < +24 dB, no click: max gain-envelope step < 0.5 dB); base checks (`check_variable_block_sizes`, `check_sr_change_mid_stream` with tol −100 dBFS (double filters re-designed), `check_state_roundtrip`, `check_param_extremes` with `ramp_params={"b3_gain_db": 0.1}`, `check_denormal_tail` with bands on, `check_channel_configs`, `check_cpu_budget`). Catch2: generated table has 7×... params; processor latency 0 at Off and == `Oversampler::latencySamples()` at 2×; `evaluateAt` equals the product of section `evaluate`s.
- [ ] **Step 2: Run, verify RED.**
- [ ] **Step 3: Implement** per ADR-0003.
- [ ] **Step 4: Run, verify GREEN** (pytest file, ctest, bench ≤ 0.5 %).
- [ ] **Step 5: Commit** `feat(channel_eq): processor with matched bands, slopes, analog Q, optional 2x`.

### Task 3: Channel EQ editor

**Files:** Create `gui/CurveDisplay.h/.cpp` (log-frequency curve from a `std::function<std::complex<double>(double)>`, draggable nodes: drag = freq/gain, wheel = Q, hold 400 ms = solo band callback), `gui/SpectrumDisplay.h/.cpp` (log-frequency bars from `SpectrumAnalyser` frames, peak hold), `gui/AdvancedDrawer.h/.cpp` (collapsible strip; open state in `uiState`), `plugins/channel_eq/ChannelEqEditor.h/.cpp`; Modify `tests/cpp/test_gui.cpp` (a second editor smoke test: create/destroy, every control attached, resize).

- [ ] **Step 1:** Failing GUI tests (editor creates/destroys, every `b<n>_*` param has a control, `CurveDisplay` node count == enabled bands, node drag changes the APVTS parameter).
- [ ] **Step 2–4:** RED → implement (mockup board 1: band cards "1 · LOW SHELF · 120 Hz · 0.0 · Q 0.7", footer "Latency 0 ms · 2× OS off", accent `accents::eq`) → GREEN; snapshot both themes with `mm_snapshot` (extend it to take a plugin name).
- [ ] **Step 5:** Commit `feat(channel_eq): editor with draggable curve and spectrum`.

### Task 4: Channel Compressor params and processor

**Files:** Create `plugins/channel_comp/{params.json, ChannelCompProcessor.h/.cpp, CMakeLists.txt}`, `tests/plugins/test_channel_comp.py`, `tests/cpp/test_channel_comp.cpp`.

**Interfaces:**
- params: `threshold_db` −60..0, `ratio` 1–20 (skew 0.4) plus `ratio_inf` bool, `attack_ms` 0.1–100 (skew 0.3), `release_ms` 10–2000 (skew 0.3), `auto_release` bool, `knee_db` 0–24, `detector` {Peak, RMS}, `topology` {FeedForward, Feedback}, `mix_pct` 0–100, `sc_hp_hz` 20–500 (+ `sc_hp_enabled`), `ext_key` bool, `makeup_db` −12..24, `auto_makeup` bool, `stereo_link_pct` 0–100, `listen_sc` bool (`ai: false`).
- `ChannelCompProcessor` with a mono/stereo sidechain bus; `float currentGainReductionDb()` atomic for the editor; `MeterFrame::gainReductionDb` filled (base: add a `setGainReductionDb(float)` hook that the base copies into the frame).

- [ ] **Step 1: Failing tests.** Python: `test_comp_ratio` (RMS detector, sines at −12/−6/0 dB peak, threshold −20: slope within 0.1 of 2/4/8/20; ∞ flat), `test_comp_attack_release` (DC steps as in M1, attack {1, 10, 100}, release {50, 300, 1000} within 10 %, 4 rates), `test_comp_knee` (gain at threshold with knee 12 equals the ADR formula ±0.1 dB), `test_comp_null_unity` (ratio 1, < −90 dBFS), `test_comp_feedback_topology` (static ratio measured ≈ labelled within 0.3 for 4:1 — feedback compresses less at the same ratio; record), `test_comp_sidechain_ext` (ext key fed with silence → no GR on a loud main; fed with loud → GR on a quiet main), `test_comp_ext_key_without_bus` (ext_key on, sidechain layout 0 → behaves as internal), `test_comp_mono_link` (mono instance with link 100 → equals link 0), `test_comp_mix` (mix 50 → output = 0.5·wet + 0.5·dry within 0.01 dB), `test_comp_auto_makeup`, `test_comp_extremes`, base checks incl. `check_cpu_budget` < 0.4 %.
- [ ] **Step 2–5:** RED → implement per ADR-0004 → GREEN → commit `feat(channel_comp): feed-forward/feedback compressor with sidechain`.

### Task 5: Channel Compressor editor

**Files:** Create `gui/TransferCurveDisplay.h/.cpp` (static curve + live input/output dot), `gui/HistoryDisplay.h/.cpp` (scrolling GR history under the curve, threshold line draggable), `gui/GainReductionMeter`, `plugins/channel_comp/ChannelCompEditor.h/.cpp`; mockup board 6 layout without the prompt bar; accent `accents::dynamics`.

- [ ] Steps as Task 3 (GUI tests, snapshots). Commit `feat(channel_comp): editor with transfer curve and GR history`.

### Task 6: Gate params and processor

**Files:** Create `plugins/gate/{params.json, GateProcessor.h/.cpp, CMakeLists.txt}`, `tests/plugins/test_gate.py`, `tests/cpp/test_gate.cpp`.

**Interfaces:** params: `threshold_db` −80..0, `range_db` −80..0, `attack_ms` 0.01–100 (skew 0.3), `hold_ms` 0–2000, `release_ms` 1–5000 (skew 0.3), `ratio` 1–100 (expander), `mode` {Gate, Expander}, `hysteresis_db` 0–12, `sc_hp_hz`/`sc_lp_hz` (+ enables), `lookahead_ms` 0–5, `ext_key`, `listen_sc`. `GateProcessor` with `dspLatencySamples()` = lookahead; `maxDspLatencySamples()` = 5 ms worth; `bool isOpen()` atomic for the editor.

- [ ] **Step 1: Failing tests.** `test_gate_open_close_times` (DC steps: attack {0.1, 1, 10} ms, release {10, 100, 1000} ms within 10 %), `test_gate_hold` (burst shorter than hold keeps the gate open for hold ± 10 %), `test_gate_range_floor` (closed gain == range ± 0.1 dB), `test_gate_hysteresis_no_chatter` (tone at threshold with ±1 dB 5 Hz AM, hysteresis 3 dB: ≤ 1 open→close transition per AM cycle; hysteresis 0 dB chatters more — recorded), `test_gate_lookahead_latency` (reported == measured; opening with 5 ms lookahead has no truncated transient: output envelope reaches 90 % before the input peak), `test_gate_expander_ratio` (2:1 below threshold within 0.1), `test_gate_sidechain_filters`, `test_gate_ext_key_without_bus`, base checks incl. `check_cpu_budget` < 0.3 %.
- [ ] **Step 2–5:** RED → implement per ADR-0005 → GREEN → commit `feat(gate): gate/expander with hysteresis, hold, lookahead, sidechain`.

### Task 7: Gate editor

**Files:** `plugins/gate/GateEditor.h/.cpp` using `HistoryDisplay` with open/closed shading, listen button. Steps as Task 3. Commit `feat(gate): editor with waveform history`.

### Task 8: Shared stability fuzz, presets, nightly

**Files:** Create `tests/plugins/test_stability.py` (every plugin folder: every parameter automated at audio rate with random values on noise, SR schedule 44.1→96→48 k and block schedule 1…4096 mid-stream; duration from `MM_FUZZ_SECONDS` (default 120, nightly 3600); asserts `nan_count == 0`, peak < +30 dB, no sample-to-sample jump > 6 dB in the gain envelope for pure-gain parameters), presets (≥ 8 per plugin: EQ "Vocal · Presence lift", "Kick · Tighten", "Bass · Clean low end", ...; Comp "Kick · Default", "Vocal · Smooth", "Mix glue"...; Gate "Snare · Tight", "Tom · Bleed", ...), Modify `.github/workflows/nightly.yml` (run the fuzz with `MM_FUZZ_SECONDS=3600` on ubuntu + macos), `tools/budgets.json` unchanged.

- [ ] Steps: failing fuzz test (initially on utility only: passes; then on the three new plugins), presets test extended to every plugin (`tests/cpp/test_presets.cpp` iterates plugins), commit `test(plugins): stability fuzz and factory presets`.

### Task 9: STATE, REVIEW, tag

- [ ] Fill `docs/STATE.md` M2 measurements (response deviations, timing errors, ratio errors, CPU per plugin, pluginval per plugin per OS, fuzz minutes), write `docs/REVIEW.md` (archive M1 to `docs/reviews/`), run `tools/run_pluginval.sh` for the three plugins (CI does all OSes), `bench_cpu.py --all`, push, CI green, tag `review/m2-channel-essentials`, merge to main, publish the review page. Print `READY FOR REVIEW: M2 Channel essentials`.
