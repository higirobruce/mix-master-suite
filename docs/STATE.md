# Project state

Updated: 2026-11-03

## Milestones

| Milestone | Status | Branch | Review tag |
| --- | --- | --- | --- |
| M0 Scaffold | built; CI run 41328502710 pending; review tag on green | milestone/m0-scaffold | review/m0-scaffold (after CI) |
| M1 Core DSP | done; CI green (run 41351360069); merged to main | milestone/m1-core-dsp | review/m1-core-dsp |
| M2 Channel essentials | done; CI run 41386855649 green; merged to main | milestone/m2-channel-essentials | review/m2-channel-essentials |
| M3 Colour and space | done; CI run 41521435810 green; merged to main | milestone/m3-colour-and-space | review/m3-colour-and-space |
| M4 Stereo and vocal | done; CI run 41655793686 green; merged to main | milestone/m4-stereo-and-vocal | review/m4-stereo-and-vocal |
| M5 Mastering | in progress (plan written 2026-11-03) | milestone/m5-mastering | — |
| M6 Delivery | planned (roadmap) | — | — |
| M7 AI layer | planned (roadmap) | — | — |

Process note (user instruction 2026-11-01): milestones run back-to-back; REVIEW.md is written and the review tag pushed at each gate, but the loop does not wait for sign-off. Reviewer notes are collected afterwards.

## M0 Scaffold — tasks

Plan: docs/superpowers/plans/2026-11-01-m0-scaffold.md

Exit criteria (CLAUDE.md, verbatim): "repo layout, CMake, CI matrix green, Utility plugin builds in all formats, pluginval level 10 passes on macOS/Windows/Linux. ADR-0001 build system, ADR-0002 parameter model."

- [x] Task 0 Toolchain, repository, branch
- [x] Task 1 ADR-0001 build system, ADR-0002 parameter model
- [x] Task 2 Root CMake, dependency pins, warnings, sanitizers
- [x] Task 3 Parameter generator tools/gen_params.py
- [x] Task 4 First core modules + Catch2 tests
- [x] Task 5 mm_plugin_base
- [x] Task 6 Utility plugin
- [x] Task 7 mm_render offline host
- [x] Task 8 Python harness and Utility measurement suite
- [x] Task 9 mm_gui shared chrome and Utility editor
- [x] Task 10 pluginval runner, CPU benchmark, CI matrix
- [x] Task 11 Factory presets, STATE, REVIEW, tag

## Measurements

| Criterion | Target | Measured | Date |
| --- | --- | --- | --- |
| Utility null test at flat settings (44.1/48/96/192 k) | < −120 dBFS | −200 dBFS (exact zero) | 2026-11-01 |
| Gain accuracy −24…+24 dB | ±0.01 dB | 0.0000 dB | 2026-11-01 |
| Phase rotation 45/90/135° at 100 Hz | ±1°, ±0.01 dB | ≤ 0.003°, 0.00 dB | 2026-11-01 |
| DC block rejection after 1.5 s | < −60 dBFS | −261 dBFS | 2026-11-01 |
| Variable block sizes vs single block | < −120 dBFS | −200 dBFS | 2026-11-01 |
| Sample-rate change mid-stream | < −120 dBFS | −200 dBFS | 2026-11-01 |
| Gain-matched bypass (param and host) | ±0.1 dB | pass | 2026-11-01 |
| State round trip / forward compat / channel configs / NaN guard | pass | pass (ctest 31/31, pytest 43/43) | 2026-11-01 |
| CPU per instance, 100 instances, 48 k / 64 | < 0.1 % | 0.055–0.058 % hosted VST3 (0.029 % in-process) | 2026-11-01 |
| pluginval strictness 10 | pass | VST3 PASS, AU PASS (macOS local) | 2026-11-01 |
| clap-validator | pass | 32 passed, 0 failed, 1 warning, 11 skipped | 2026-11-01 |
| auval | pass | AU VALIDATION SUCCEEDED | 2026-11-01 |
| ASan/UBSan (Debug) | no report | ctest 31/31, pytest plugins pass under sanitizers | 2026-11-01 |
| CI matrix macOS/Windows/Linux | green | run 41328502710 in progress | 2026-11-01 |

## M1 Core DSP — tasks

Plan: docs/superpowers/plans/2026-11-01-m1-core-dsp.md

Exit criteria (CLAUDE.md, verbatim): "filters (biquad, matched, FIR, Linkwitz–Riley), dynamics core, smoothing, metering FIFO, delay line, oversampling, nonlinear/ADAA. Each with unit tests, frequency/step-response measurement tests, sample-rate matrix. Null test residual < −120 dBFS for linear modules."

- [x] Task 1 Math helpers, sample-rate matrix fixtures
- [x] Task 2 Smoother exponential mode
- [x] Task 3 Biquad TDF-II + RBJ designs + analytic evaluate
- [x] Task 4 Matched (Vicanek) biquads
- [x] Task 5 TPT SVF
- [x] Task 6 One-pole / all-pass coverage
- [x] Task 7 FIR design + direct-form FIR
- [x] Task 8 Linkwitz–Riley LR2/4/8
- [x] Task 9 Detector + envelope follower
- [x] Task 10 Gain computer, lookahead, sidechain filter, compressor chain
- [x] Task 11 Fractional delay line + tempo sync
- [x] Task 12 Oversampler
- [x] Task 13 Waveshapers + ADAA
- [x] Task 14 Tape hysteresis + tube transfer
- [x] Task 15 Correlation + spectrum
- [x] Task 16 Module registry matrix
- [x] Task 17 Benchmarks, STATE, REVIEW

## M1 measurements

| Criterion | Target | Measured | Date |
| --- | --- | --- | --- |
| Biquad (7 RBJ types) vs analytic, 20 Hz–min(20 k, 0.45 fs), 4 rates | ±0.1 dB / ±2° | pass in double (C++ 65k-point IR; Python 172 cases vs scipy, double path). Float coefficients at 20 Hz / Q 10 / 192 k deviate up to ~4 dB (recorded): EQ filters run in double (ADR-0003) | 2026-11-01 |
| Matched biquads vs own transfer; DC/Nyquist/f0 equal analog; f0 sweep continuity | ±0.1 dB / ±2°; 1e-6 dB; < 0.5 dB per 1 % step | pass; matched < 0.5 dB vs analog at 10 kHz/44.1 k where RBJ > 0.5 dB; high shelf +12 dB continuous to 20 kHz | 2026-11-01 |
| SVF 8 modes vs analytic; 10 s Q 10 cutoff modulation | ±0.1 dB / ±2°; finite | pass | 2026-11-01 |
| One-pole / all-pass vs analytic | ±0.1 dB / ±2°; all-pass flat ±0.01 dB | pass (48 Python cases) | 2026-11-01 |
| FIR: Kaiser 255 taps, DC 1.0, −80 dB stopband, 0.01 dB ripple; vs scipy firwin | pass | pass; float path null < −120 dBFS | 2026-11-01 |
| Linkwitz–Riley LR2/4/8 vs Butterworth² reference; low+high all-pass | ±0.1 dB / ±2°; ±0.01 dB | pass (double crossovers) | 2026-11-01 |
| Envelope attack/release 10–90 % (0.1–100 ms / 10–2000 ms), 4 rates | within 10 % | pass; DC-step compressor chain 1/10/100 ms and 50/300/1000 ms within 10 % | 2026-11-01 |
| Static ratio 2/4/8/20/∞ (RMS detector) | within 0.1 | pass, tolerance 0.1 absolute at every ratio (∞: slope < 0.005); 1:1 null < −90 dBFS | 2026-11-01 |
| Lookahead latency reported == measured; no overshoot above threshold + 0.5 dB | pass | pass (240 samples at 5 ms / 48 k) | 2026-11-01 |
| Fractional delay echo time, 4 rates | ±0.05 ms | pass (Lagrange3 and Thiran1) | 2026-11-01 |
| Oversampler latency exact; passthrough flat to 0.4 fs | ±0; ±0.01 dB | pass for 1/2/4/8× at 4 rates | 2026-11-01 |
| Alias gate 19 kHz, tanh +6 dB, ADAA1 (double antiderivatives) | < −80 dBFS | 8×: −121 / −102 / −109 / −141 at 44.1/48/96/192 k (gated); 4×: −79.7 / −82.5 / −113 / −142 (recorded; 44.1 k sits at the gate) | 2026-11-01 |
| Alias at +18 dB (full drive), 4× + ADAA1, tanh | < −80 dBFS (spec) | **not met**: −67 / −44 / −68 / −105 at 44.1/48/96/192 k (spec question for M3) | 2026-11-01 |
| Tube alias 4× (drive 1, bias 0.5) | < −80 dBFS | −92 / −98 / −136 | 2026-11-01 |
| Tape alias (drive/sat/width 0.5) | < −80 dBFS | 8×: −89 / −90 / −91; 4×: −68 / −80 / −90 | 2026-11-01 |
| Linear modules block-size independent (schedule 16/4096/1/500/63) | < −120 dBFS | pass (9 modules × 2 rates) | 2026-11-01 |
| Every module: silence→silence, noise→finite, 4 rates | pass | pass (17 modules) | 2026-11-01 |
| Core cost ns/sample @48 k (Release, M2 Max) | — | biquad_float 7.6, biquad_double 7.6, svf_float 7.0, lr4_double 13.9, fir_255_float 388.2, smoother 1.5, compressor_chain 13.7, lookahead_5ms 20.5, fractional_delay_lagrange3 5.8, tanh_adaa 13.3, tape_hysteresis 310.9, tube 3.6, rms_meter 2.1, correlation 1.5 | 2026-11-01 |

## M2 Channel essentials — tasks

Plan: docs/superpowers/plans/2026-11-01-m2-channel-essentials.md

Exit criteria (CLAUDE.md, verbatim): "Channel EQ, Channel Compressor, Gate. Frequency response within ±0.1 dB / ±2°; attack/release within 10%; ratio within 0.1; CPU within budget table." Spec phase 2 exit: "null tests against reference transfer curves within 0.1 dB."

- [x] Task 1 ADR-0003 EQ bands and analog Q, ADR-0004 compressor topology, ADR-0005 gate hysteresis/hold/lookahead
- [x] Task 2 Channel EQ params and processor
- [x] Task 3 Channel EQ editor (curve, spectrum, drawer, snapshot tool)
- [x] Task 4 Channel Compressor params and processor
- [x] Task 5 Channel Compressor editor (transfer curve, GR history, GR meter)
- [x] Task 6 Gate params and processor
- [x] Task 7 Gate editor (history with open/closed shading)
- [x] Task 8 Stability fuzz, factory presets, nightly workflow
- [x] Task 9 STATE, REVIEW, pluginval, bench, CI, tag

## M2 measurements

| Criterion | Target | Measured | Date |
| --- | --- | --- | --- |
| Channel EQ: every band type and slope vs its design (independent Python mirror), 20 Hz–0.45 fs, 44.1/48/96/192 k | ±0.1 dB / ±2° | 120 cases: max deviation 0.000 dB / 0.000° (double-precision biquads, designs mirrored in `tests/harness/designs.py`) | 2026-11-02 |
| Channel EQ: composed 7-band response vs independent `eq_ref.py` | ±0.1 dB / ±2° | pass (ADR-0003 slopes: 6/12/18/24/36/48 dB/oct Butterworth; −3 dB at fc checked) | 2026-11-02 |
| Channel EQ null at flat settings, 4 rates | < −120 dBFS | −200 dBFS (exact) | 2026-11-02 |
| Channel EQ 2× oversampling vs analog prototype to 0.4 fs | ±0.1 dB | pass; 1× vs 2× difference 0.11 dB at 0.28 fs @ 44.1 k (recorded) | 2026-11-01 |
| Channel EQ band sweep (+12 dB bell, 20 Hz–20 kHz in 2 s across a 1 kHz tone) | 1 ms envelope step < 0.5 dB | 0.11 dB | 2026-11-02 |
| Channel EQ HP/LP cutoff sweeps (a decade in 2 s, every slope, block 512) | step < 0.5 dB | ≤ 0.002 dB (was a −31 dBFS burst at 6/18 dB/oct: the one-pole was reset on every redesign) | 2026-11-02 |
| Channel EQ band gain → 0 dB and band on/off (blocks 64/512/2048) | glide, never a step (< 1.5 dB/ms) | 0.34–0.62 dB per ms (20 ms glide; was a −24 dBFS click) | 2026-11-02 |
| Channel EQ HP on/off and slope change while running (12/24/48 dB/oct at fc on a tone at fc) | glide (< 2 dB/ms) | 0.52–0.84 dB per ms; worst resonant release +1.0 dB at 48 dB/oct (was +5.5 dB) | 2026-11-02 |
| Channel Compressor feedback topology ratio 2/4/8; makeup independence | within 0.3; 12 dB makeup = +12 dB out | 2.000 / 4.000 / 8.000 (FB out == FF out at 0 dB in); +12.000 dB | 2026-11-02 |
| Channel Compressor mix 100→0 and auto-makeup on (blocks 64/2048) | per-sample ramps (< 2.5 / < 1 dB per ms) | mix 0.63–1.24 dB/ms (linear crossfade of a 15 dB difference over 20 ms); auto makeup 0.27–0.38 dB/ms | 2026-11-02 |
| Gate lookahead 0→5 ms while a tone runs | no sample jump (< 1.5× the tone's own) | pass (5 ms tap crossfade); latency 240 reported | 2026-11-02 |
| Channel Compressor static ratio 2/4/8/20 (RMS detector) and ∞ | within 0.1 | 2.000 / 4.000 / 8.000 / 20.000; ∞ slope < 0.05 dB over 6 dB | 2026-11-02 |
| Channel Compressor attack 1/10/100 ms, release 50/300/1000 ms on DC steps, 4 rates | within 10 % | attack 0.998–98.9 ms (worst −1.1 %), release 50.0 / 300 / 1000.0 ms | 2026-11-02 |
| Channel Compressor knee (12 dB, 4:1), null at 1:1, mix, auto makeup, link, feedback | per test | knee −1.125 dB at threshold; 1:1 null < −90 dBFS; mix linear ±2e-4; auto makeup +7.5 dB; FB compresses less (−7.3 vs −12.7 dB out at 0 dB in) | 2026-11-02 |
| Gate attack 0.1/1/10 ms, release 10/100/1000 ms on DC steps, 4 rates | within 10 % | attack 0.091–0.104 / 0.998–1.0 / 10.0 ms; release 10.0 / 100.0 / 1000.0 ms | 2026-11-02 |
| Gate hold 50/200/1000 ms (burst shorter than hold) | within 10 % | 51 / 201 / 1001 ms (+1 ms: 2 ms key peak hold, ADR-0005) | 2026-11-02 |
| Gate range floor −20/−40/−80 dB | ±0.1 dB | pass (output = input + range) | 2026-11-02 |
| Gate hysteresis: 1 kHz tone ±1 dB AM at 5 Hz around the threshold, 3 s | ≤ 1 open→close per cycle at 3 dB | 0 transitions at 3 dB; 15 at 0 dB (recorded) | 2026-11-02 |
| Gate lookahead 5 ms: reported == measured; onset preserved | exact; ≥ 0.9 | 240 samples both; first-ms onset ratio 1.00 (0.30 without lookahead) | 2026-11-02 |
| Gate expander ratio 2/4 below threshold | within 0.1 | 2.000 / 4.000 | 2026-11-02 |
| Sidechain HP/LP, external key with silent fallback, listen (comp + gate) | pass | pass | 2026-11-02 |
| Base checks per plugin: variable blocks, SR change mid-stream, state round trip, param extremes, denormal tail, channel configs | < −100 / −60 dBFS; pass | EQ −200 / −200; comp −200 / −126; gate −200 / −200; all others pass | 2026-11-02 |
| Stability fuzz, every parameter every 50 ms, SR 44.1→96→48 k and block 1…4096 mid-stream, 120 s per plugin | no NaN, peak < input + max static gain + 24 dB | pass ×4; peaks: utility +24.9, EQ +49.6 (bound 114: five +18 dB bells), comp +19.7, gate +11.3 dBFS; pure-gain steps ≤ 0.06 dB/sample | 2026-11-02 |
| CPU per instance, 100 instances, 48 k / 64 (hosted VST3, M2 Max) | EQ < 0.5 %, comp < 0.4 %, gate < 0.3 % | local M2 Max: EQ 0.122 %, comp 0.260 %, gate 0.167 %; CI runners (run 41386855649) macOS 0.090 / 0.192 / 0.121, Linux 0.129 / 0.288 / 0.164, Windows 0.138 / 0.249 / 0.168 (utility 0.041–0.061). Per-sample pow/log10 replaced by tested fast conversions after Windows measured comp 0.50 / gate 0.31 | 2026-11-02 |
| pluginval strictness 10, auval, clap-validator (macOS local) | pass | EQ, comp, gate: VST3 PASS, AU PASS, CLAP PASS | 2026-11-02 |
| Factory presets | ≥ 8 per plugin | EQ 10, comp 10, gate 9, utility 8; every preset loads (C++ check per plugin) | 2026-11-02 |
| CI matrix macOS/Windows/Linux + sanitizers | green | run 41386855649: all five jobs green (build-test ×3 incl. pluginval/clap-validator for every plugin and the CPU benchmark; sanitizers ×2) | 2026-11-02 |

## M3 Colour and space — tasks

Plan: docs/superpowers/plans/2026-11-02-m3-colour-and-space.md

Exit criteria (CLAUDE.md, verbatim): "Saturation (aliasing < −80 dBFS at 19 kHz, 4× OS), Delay, Reverb (FDN + Dattorro plate). Reverb decay time measured within 10% of labelled RT60."

- [x] Task 1 ADR-0006 saturation stages, ADR-0007 delay topology, ADR-0008 reverb design
- [x] Task 2 Saturation params and processor
- [x] Task 3 Saturation editor (harmonic display, character tabs)
- [x] Task 4 Delay params and processor (+ render host BPM)
- [x] Task 5 Delay editor (tap display with tempo grid)
- [x] Task 6 Reverb core modules (FDN-16, Dattorro plate, early reflections)
- [x] Task 7 Reverb params and processor
- [x] Task 8 Reverb editor (decay display)
- [x] Task 9 Presets, fuzz, listening sheet
- [x] Task 10 STATE, REVIEW, review, CI, tag

## M3 measurements

| Criterion | Target | Measured | Date |
| --- | --- | --- | --- |
| Saturation alias floor below 19 kHz, 19 kHz tone at −12 dBFS, 4× + ADAA, **reference drive** (1 % THD: Tape 0 dB, Tube/Transistor 2.96 dB, Clip 8.79 dB), 4 rates × 4 characters | < −80 dBFS (ADR-0006 ruling; spec says "full drive") | worst −96.3 dBFS (16 cases; best −141.6) | 2026-11-02 |
| Saturation alias floor at +12 dB drive (recorded) | — | −93.8 … −145.2 dBFS | 2026-11-02 |
| Saturation alias floor at maximum drive 36 dB (recorded, not gated) | spec: < −80 | −54.1 (Tube, 44.1 k) … −119.1 dBFS; Tape/Clip/Transistor at 96/192 k pass, 44.1/48 k fail (see REVIEW question 1) | 2026-11-02 |
| Saturation THD at the reference drive | 1.0 ± 0.25 % | 1.01–1.18 % | 2026-11-02 |
| Saturation even/odd balance vs bias (even−odd energy, dB) at bias −1/−0.5/0/0.5/1 | monotonic, none at 0 | Tube 20.5 / 13.3 / −151 / 13.3 / 20.5; Transistor 5.5 / −1.6 / −152 / −1.6 / 5.5; Clip 5.6 / 0 / −152 / 0 / 5.6; Tape 5.7 / −2.3 / −156 / −2.3 / 5.7 | 2026-11-02 |
| Saturation latency reported == measured (4× oversampler), 4 rates | exact | pass (40 samples at 48 k = 0.83 ms) | 2026-11-02 |
| Saturation unity small-signal gain (−60 dBFS tone, drive 0–36) / mix 0 null / bounded + DC-free at max drive | ±0.5 dB / < −90 dBFS / < +6 dBFS, DC < −60 | pass / pass / pass (5 Hz DC blocker) | 2026-11-02 |
| Oversampler (rewritten): latency exact and passthrough flat for 1/2/4/8× at 4 rates; 8× alias ≥ 50 dB over 1× and < −80 | M1 gates | pass (C++ 128 assertions, Python); 4× stereo frame 55 ns (was ~300) | 2026-11-02 |
| Tabulated ADAA vs analytic antiderivative / output | 1e-9 / 1e-6 | pass | 2026-11-02 |
| Delay echo time 1 / 37.5 / 375 / 4000 ms, 4 rates | ±0.05 ms | error −0.011 … +0.006 ms (16 cases) | 2026-11-02 |
| Delay tempo sync (1/8, 1/4 dotted, 1/8 triplet at 120 BPM; 1/4 at 90) | ±0.1 ms | pass | 2026-11-02 |
| Delay feedback decay fb^n over 6 taps / soft limit at 120 % / ping-pong alternation / filter in loop / modulation ±5 ms / ducking −12 dB | ±0.2 dB / < +6.1 dBFS / alternates / ≥ 20 dB / ±10 % / ±1 dB | 0.026 dB / pass / pass / pass / pass / pass | 2026-11-02 |
| Delay time jump 1 → 4000 → 1 ms while a tone runs | no sample jump > 2× input's | pass (20 ms equal-power tap crossfade) | 2026-11-02 |
| Reverb RT60 (Schroeder, −5…−35 dB) at 0.5 / 2 / 8 s, Room / Hall / Plate / Chamber, 4 rates, damping 0 | within 10 % | worst Room 3.5 %, Hall 3.2 %, Chamber 3.2 %, Plate 0.5 % (48 cases; core modules 52 cases same) | 2026-11-02 |
| Reverb damping: T60 at 8 kHz vs 500 Hz at damping 100 % | ≥ 2× shorter | FDN 0.25 vs 1.28 s; plate 0.82 vs 1.91 s | 2026-11-02 |
| Reverb 20 s decay, size 100, diffusion 100: 1 s-window energy strictly decreasing / pre-delay 0–250 ms / mono→stereo decorrelation / width 0 identical / algorithm switch | pass / ±1 ms / |corr| < 0.5 / exact / 10 ms-envelope step < 3 dB | pass / pass / pass / pass / 1.48 dB (200 ms morph) | 2026-11-02 |
| Base checks ×3 (variable blocks, SR change, state, extremes, denormals, channel configs) | pass | pass; SR-change residuals: saturation −89.5, delay −200, reverb −96.3 dBFS | 2026-11-02 |
| Stability fuzz 120 s per plugin (7 plugins) | no NaN, bounded | pass for all 7 plugins (120 s each, 4 workers) | 2026-11-03 |
| CPU per instance, 100 instances, 48 k / 64 (hosted VST3) | Saturation < 1.0 %, Delay < 0.5 %, Reverb < 2.0 % | local M2 Max: saturation 0.840 %, delay 0.283 %, reverb 0.799 % (after the review fix pass); CI: Linux (factor 1.90) sat 0.897 / delay 0.342 / reverb 0.943; Windows (1.35) 1.107 / 0.390 / 1.093; macOS (1.23) 0.940 / 0.298 / 0.847 — raw percentages; the gate scales by the runner factor, and the saturation's raw figure exceeds 1.0 on the slowest Windows runner | 2026-11-03 |
| pluginval strictness 10, auval, clap-validator (macOS local) | pass | saturation, delay, reverb: VST3, AU, CLAP PASS | 2026-11-03 |
| Factory presets | ≥ 8 per plugin | saturation 10, delay 9, reverb 10 (loaded by the C++ checks) | 2026-11-03 |
| CI matrix + sanitizers | green | run 41521435810: all five jobs green (build-test ×3 with pluginval/clap-validator for every plugin, sanitizers ×2) | 2026-11-03 |

## M4 Stereo and vocal — tasks

Plan: docs/superpowers/plans/2026-11-03-m4-stereo-and-vocal.md

Exit criteria (CLAUDE.md, verbatim): "De-esser, Stereo Imager. Mono-compatibility and correlation tests."

- [x] Task 1 ADR-0009 de-esser, ADR-0010 imager
- [x] Task 2 De-esser params and processor (3c2a5af)
- [x] Task 3 De-esser editor (feea0bd)
- [x] Task 4 Imager params and processor (a5c96af; ADR-0010 amended: all-pass legs as single biquads, 64-bit crossovers, crossfaded mono-below)
- [x] Task 5 Imager editor (1007d61)
- [x] Task 6 Presets, fuzz, STATE, REVIEW (review, CI, tag below)
- [x] Task 7 CPU-gate fix after CI run 41549350883 (macOS: utility 0.1014 % vs limit 0.100 at runner factor 0.93; Windows: de-esser 0.552 % vs limit 0.546 at factor 1.36). Root causes: one 5 s sample each for the calibration and the plugin (the macOS calibration read 25 % faster and Utility 20 % slower than the run before), and the de-esser sitting at 95 % of its budget on the reference machine after the review fix pass. Fixes: both benches take the minimum of 3 × 3 s (reference re-measured 0.364); de-esser −8.5 % on the reference (lookahead sliding max by block decomposition, SVF redesign without tan/pow, block copies through the lookahead ring, raw channel pointers); fastExp2 rounds without a floor() call (a library call on MSVC's SSE2 baseline, three per sample in the de-esser and gate); CI runs pluginval on every plugin (it listed four). Verified locally: ctest 129/129, plugin suite 95 + 9 CPU gates; the gate's denormal-tail check (a two-timing comparison) failed once under `-n auto` with the machine loaded and passed alone. The 0.348 % was measured idle before three self-review fixes that leave the hot path unchanged (pointer table covers inputs and outputs, the SVF's tan guard sits in update(), fastExp2 rounds by a truncating int conversion).

## M4 measurements

| Criterion | Target | Measured | Date |
| --- | --- | --- | --- |
| De-esser: 1 kHz component untouched while the 6 kHz band is reduced, Band and Split, 4 rates | within ±0.05 dB; band < −6 dB | 1 kHz −0.026 … −0.029 dB; band Band −17.7 dB, Split −10.6 … −11.2 dB (12 dB range on a −12 dBFS 6 kHz tone, threshold −24) | 2026-11-03 |
| De-esser: no reduction below threshold / range cap 6 dB | 0 dB / 6 ± 0.2 | pass / Band 5.98, Split 5.91 dB | 2026-11-03 |
| De-esser: Split mode low + high sums flat / Listen = dry − wet | ±0.05 dB / null | pass / pass | 2026-11-03 |
| De-esser: lookahead 0 / 1 / 3 ms latency reported == measured, 4 rates; change 0 → 3 ms while running | exact; no sample jump > 1.5× input's | pass (144 samples at 48 k for 3 ms); pass (5 ms tap crossfade) | 2026-11-03 |
| De-esser: base checks (variable blocks, SR change, state, extremes, denormals, channel configs) | pass | pass; variable-block and SR-change residuals −200 dBFS | 2026-11-03 |
| Imager: width 0 → L = R | correlation 1 ± 0.001 | pass | 2026-11-03 |
| Imager: width 100 → the LR4 all-passed input, 4 rates | null < −100 dBFS; flat ±0.02 dB at 50 Hz … 15 kHz | pass at 44.1 / 48 / 96 / 192 k (64-bit crossovers; float left 192 k at −83 dBFS) | 2026-11-03 |
| Imager: width 200 per band, pure side tone ≥ 2 octaves inside its band (40 / 800 / 12 000 Hz) | +6.02 ± 0.1 dB | +5.98 / +5.97 / +6.01 dB | 2026-11-03 |
| Imager: mono-below (LR4 on S) | side 2.9 octaves below < −60 dB; 2 kHz unchanged ±0.05 | −70.0 dB (40 Hz under 300 Hz); −41.9 dB at 1.7 octaves (60 Hz under 200 Hz, recorded); 2 kHz pass | 2026-11-03 |
| Imager: mono compatibility — L + R unchanged for width 0 / 50 / 150 / 200, per band | ±0.1 dB vs the all-passed reference | pass | 2026-11-03 |
| Imager: pan laws (−100 / +100 / 50), mono input (no side: every width = all-passed input; pan 100 silences L) | constant power; null < −100 dBFS | pass / pass | 2026-11-03 |
| Imager: width 0 → 200 sweep, mono-below on/drag/off, crossover drags while tones run | no sample jump above 1.1× the static output's own slope (2× for mono-below) | pass / pass (20 ms engagement crossfade, 5 ms/oct glide) / pass | 2026-11-03 |
| Imager: correlation meter on known pairs ρ = 1 / 0.5 / 0 / −1 | ±0.02 | pass (300 ms exponential window, 2 s per pair) | 2026-11-03 |
| Imager: base checks | pass | pass; variable-block and SR-change residuals −200 dBFS | 2026-11-03 |
| Stability fuzz 120 s (both) + pure-gain envelope (imager measured against its all-pass reference) | no NaN, bounded, no gain step > 6 dB | pass | 2026-11-03 |
| CPU per instance, 100 instances, 48 k / 64 (hosted VST3), local M2 Max, min of 3 × 3 s | de-esser < 0.4 %, imager < 0.4 % | de-esser 0.348 % (0.377–0.386 after the review fix pass, before the CPU-gate fix), imager 0.179 %; gate 0.157 %, utility 0.056 %, channel_comp 0.272 % unchanged or better | 2026-11-03 |
| pluginval strictness 10, auval, clap-validator (macOS local) | pass | de-esser and imager: VST3, AU, CLAP PASS | 2026-11-03 |
| Factory presets | ≥ 8 per plugin | de-esser 10, imager 11 (incl. Default; loaded by the C++ checks) | 2026-11-03 |

## M5 Mastering — tasks

Plan: docs/superpowers/plans/2026-11-03-m5-mastering.md

Exit criteria (CLAUDE.md, verbatim): "Linear-phase FIR engine, Linear-phase EQ, Multiband comp, Bus comp, M/S processor, Soft clipper, True-peak limiter. EBU Tech 3341/3342 vectors within ±0.1 LU and ±0.3 dBTP; limiter never exceeds ceiling by more than 0.1 dBTP on the test set."

- [x] Task 1 ADR-0011 and the linear-phase FIR engine (partitioned convolver, linear-phase design) (34e7269, b1c8411, dc9a0cf)
- [x] Task 2 Loudness and true-peak metering with the EBU vectors (39e523f)
- [x] Task 3 Linear-phase EQ (eedba70, 2d161e7)
- [x] Task 4 Multiband compressor (ef108ad, 1dd2f50)
- [x] Task 5 Bus compressor
- [x] Task 6 M/S processor (079dc33, 2b1d801)
- [x] Task 7 Soft clipper (6216f01, daefe1d)
- [x] Task 8 ADR-0012 and the true-peak limiter (1aa9d4f, cacd82a)
- [x] Task 9 Presets, fuzz, chain integration, STATE, REVIEW, review (fix pass: de04adf, 38da030, 0f99ba7, 14b6686, 1c68c80), CI, tag

## M5 measurements

| Criterion | Target | Measured | Date |
| --- | --- | --- | --- |
| FIR engine: partitioned convolver vs direct convolution, 1023 / 4097 / 16383 taps, varying block schedule (1 … 4096) | < −120 dBFS | pass (float output against a double direct form); output impulse at P + (N−1)/2 exactly; silent after reset | 2026-11-03 |
| FIR engine cost, Release, M2 Max, P = 512 (ns per sample per channel) | fits 2.5 % at 16384 taps | convolver_4096_p512 115, convolver_16384_p512 214 (2.1 % of a stereo 48 k frame at the longest FIR) | 2026-11-03 |
| Linear-phase design: three bands at 4 rates, 2048 and 16384 taps, bands ≥ 2 floors | ±0.1 dB above 2 × floor; phase ±2° | worst 0.015 dB (Tukey half taper; no window rings 0.57 dB from a sub-floor band, Kaiser β 8 costs 0.4 dB) | 2026-11-03 |
| Loudness meter: BS.1770-4 Tables at 48 k; synthetic Tech 3341 seq 1 / 2 / 3 / 4 / 5 / 9 at 4 rates; Tech 3342 seq 1–4 | 1e-5; ±0.1 LU; LRA ±1 LU | tables to 1e-9 (De Man form); every synthetic sequence within 0.1 LU at 44.1 / 48 / 96 / 192 k; LRA 10 / 5 / 20 / 15 within 1 LU; pyloudnorm cross-check within 0.1 LU | 2026-11-03 |
| True-peak meter: fs/4 inter-sample peak (sample peak −9.03, true −6.02 dBTP), 1 kHz at −1 dBFS, 4 rates | ±0.3 dBTP (spec); ±0.1 asserted | pass at ±0.1 dBTP | 2026-11-03 |
| EBU real files (seq-3341-7/8, 3342-5/6, TP 15–23) | ±0.1 LU / ±0.3 dBTP | not run: the EBU serves the set behind a browser challenge; manual two-step in tests/fixtures/ebu/README.md (reviewer question) | 2026-11-03 |
| LP-EQ: three linear bands, 4 rates, 4096 and 16384 taps | ±0.1 dB above 2 × floor; phase ±2° | pass (impulse centred on the reported latency) | 2026-11-03 |
| LP-EQ: Minimum mode nulls against the Channel EQ (matched designs), 44.1 / 96 k | < −120 dBFS | pass | 2026-11-03 |
| LP-EQ: all bands off = input delayed by the reported latency, every length, 4 rates; latency = P + (L−2)/2 | < −120 dBFS; exact | pass; 512 + 2047 at 4096 taps with 64-sample blocks (4096 + 2047 with 4096-sample blocks) | 2026-11-03 |
| LP-EQ: FIR length 2048 → 16384 while a +6 dB band plays a tone | no sample step above 1.1 × the static output's own; level unchanged ±0.1 dB | pass (20 ms crossfade inside the convolver; the reported latency jumps at the swap) | 2026-11-03 |
| LP-EQ: Mid-only / Side-only band isolation | untouched lane < −120 dBFS; targeted lane +6.0 ± 0.1 dB | pass | 2026-11-03 |
| LP-EQ: base checks (variable blocks, SR change, state, extremes, denormals, channel configs) | pass | pass | 2026-11-03 |
| LP-EQ CPU, 100 instances, 48 k / 64, min of 3 × 3 s | < 2.5 % | 1.02 % at 4096 taps, 0.93 % at 16384, 0.88 % at 2048 (loaded machine, load average 11; CPU gate passed) | 2026-11-03 |
| LP-EQ pluginval / auval / clap-validator (macOS local) | pass | VST3, AU, CLAP PASS (clap-validator 32 passed, 0 failed, 1 warning, 11 skipped) | 2026-11-03 |
| Multiband: every band 1:1 at 3 / 4 / 5 bands, 4 rates = the LR4 all-pass of every crossover (module reference) | null < −100 dBFS; latency 0 | pass (12 cases) | 2026-11-03 |
| Multiband: per-band ratio 2 / 4 / 8 / 20 on every band (tone ≥ 2 octaves inside, band soloed) | within 0.1 | pass (20 cases); on the summed output the neighbours' −66 … −80 dB stopband residual bends 20:1 by up to 0.9 (recorded, inherent to LR4 multibands) | 2026-11-03 |
| Multiband: band isolation (−6 dBFS tone in band 1, band 3 at −30 / 20:1 with a −40 dBFS tone) | band 3 tone unchanged ±0.1 dB | pass | 2026-11-03 |
| Multiband: attack 10 / 100 ms, release 100 / 1000 ms on a 2 kHz burst, peak detector, against the Channel Compressor on the same burst | within 10 % + one cycle of each other; reduction within 0.5 dB | pass (a peak detector reads ~17 ms for a 10 ms label on a tone for either; M2's 10 % gate used DC steps) | 2026-11-03 |
| Multiband: crossover 2 dragged below crossover 1 | held 1/3 octave above; sum still the clamped all-pass < −100 dBFS | pass | 2026-11-03 |
| Multiband: solo / bypass | soloed band alone (others < −70 dB); bypassed band passes ±0.3 dB | pass | 2026-11-03 |
| Multiband: base checks | pass | pass | 2026-11-03 |
| Multiband CPU, 100 instances, 48 k / 64, 4 bands, min of 3 × 3 s | < 1.5 % | 1.018 % (runs 1.023 / 1.018 / 1.019) | 2026-11-03 |
| Multiband pluginval / auval / clap-validator (macOS local) | pass | VST3, AU, CLAP PASS | 2026-11-03 |
| Bus comp: stepped attack 0.1 / 0.3 / 1 / 3 / 10 / 30 ms on a DC step, 4 rates | within 10 % + two samples | pass (24 cases) | 2026-11-03 |
| Bus comp: stepped release 0.1 / 0.3 / 0.6 / 1.2 s on a DC step | within 10 % + one sample | pass; Auto recorded (program-dependent 60 ms → 500 ms) | 2026-11-03 |
| Bus comp: stepped ratio 2 / 4 / 10 on DC levels | within 0.1 | pass | 2026-11-03 |
| Bus comp: null at the +15 dB threshold, 4 rates | < −90 dBFS, latency 0 | pass | 2026-11-03 |
| Bus comp: attack / ratio / release switched during 12 dB of reduction; 2× switched on while running | no step above 1.5 × the static output's own; latency re-reported; impulse at the latency | pass (10 ms crossfade after the oversampled path's warm-up) | 2026-11-03 |
| Bus comp: sidechain HP 120 Hz | 40 Hz tone not reduced, 1 kHz reduced | pass | 2026-11-03 |
| Bus comp: base checks | pass | pass | 2026-11-03 |
| Bus comp CPU, 100 instances, 48 k / 64, min of 3 × 3 s | < 0.5 % | 0.124 % (2× on: 0.126 %; runs 0.124 / 0.371 / 0.150 on a loaded machine; gate passed) | 2026-11-03 |
| Bus comp pluginval / auval / clap-validator (macOS local) | pass | VST3, AU, CLAP PASS | 2026-11-03 |
| M/S: encode / decode null at 4 rates; mid / side gains ±0.02 dB with the other channel untouched < −100 dBFS | < −120 dBFS; exact | pass | 2026-11-03 |
| M/S: width 0 → correlation 1; 200 → side +6.02 ± 0.1; L + R unchanged < −100 dBFS at 0 / 50 / 150 / 200 | imager semantics | pass | 2026-11-03 |
| M/S: bell / low shelf / high shelf on mid and on side vs the matched-biquad reference, 4 rates; other channel untouched | ±0.1 dB / ±2°; < −100 dBFS | pass (24 cases) | 2026-11-03 |
| M/S: mono-below (side 2.9 oct below < −60 dB; 2 kHz unchanged), solo, width sweep and mono-below switch click-free | as the imager | pass | 2026-11-03 |
| M/S: base checks; correlation meter ±0.02 on ±1 pairs | pass | pass | 2026-11-03 |
| M/S CPU, 100 instances, 48 k / 64, min of 3 × 3 s | < 0.5 % | 0.135 % (idle, bench_cpu --all) | 2026-11-03 |
| M/S pluginval / auval / clap-validator (macOS local) | pass | VST3, AU, CLAP PASS | 2026-11-03 |
| Soft clipper: ceiling never exceeded on +6 dBFS noise and a +6 dBFS 10 kHz tone, 3 ceilings × 3 knees × 4× / 8× × 4 rates; sample ceiling with the compensation off | true peak ≤ ceiling + 0.01 dB; sample peak ≤ ceiling | pass (72 cases); a full-scale 0.45 fs burst exceeds by the clipped fundamental's 4/π (+1.4 … +1.8 dB, recorded; inherent to filtering a clipped near-Nyquist tone) | 2026-11-03 |
| Soft clipper: 19 kHz at the reference drive, 4× and 8×, 4 rates | < −80 dBFS | pass (full-drive floors recorded) | 2026-11-03 |
| Soft clipper: knee curve (hard and 50 %), delta = dry − wet, latency per factor, oversampling switch, level flatness below the knee | exact; null; exact; click-free; ±0.2 dB (ADAA's two-point average droops 0.14 dB at 10 kHz at 4× / 44.1 k) | pass | 2026-11-03 |
| Soft clipper: base checks | pass | pass | 2026-11-03 |
| Soft clipper CPU, 100 instances, 48 k / 64, min of 3 × 3 s | < 1.5 % | 0.626 % (idle, bench_cpu --all) | 2026-11-03 |
| Soft clipper pluginval / auval / clap-validator (macOS local) | pass | VST3, AU, CLAP PASS | 2026-11-03 |
| Limiter: ceiling never exceeded on 7 synthetic worst cases × 4 configurations (characters, releases, auto) × 3 ceilings × 3 lookaheads × 4 rates | ≤ ceiling + 0.1 dBTP | pass on every case measured from the output's first sample (lookahead window floored at 0.25 ms, release floors 1 / 2 ms, sample clamp after the down-sampler: ADR-0012 amendments); white noise 7–13 dB over recorded at 0.2–0.4 dBTP over | 2026-11-03 |
| Limiter: release stages per character (500 ms burst → slow stage ±25 %; 5 ms burst → fast stage ±50 %) | labelled | pass: slow 299 / 492 / 150 ms for 300 / 500 / 150 (gate 10 %); fast 34 / 10 / 57 ms for 30 / 10 / 50 (gate 25 %: the slow stage's 4 % residual; reviewer question 6) | 2026-11-03 |
| Limiter: null below the ceiling vs the oversampler passthrough, latency per lookahead, lookahead change, 19 kHz aliasing, dither (16 / 24-bit quantisation, −96.3 dBFS TPDF floor), link law | < −100 dBFS; exact; click-free; < −80 dBFS; exact / ±1 dB; pass | pass (null aligned by the reported latency; latency 12 + 45 at lookahead 0 / 48 k, 141 at 2 ms) | 2026-11-03 |
| Limiter: base checks | pass | pass | 2026-11-03 |
| Limiter CPU, 100 instances, 48 k / 64, min of 3 × 3 s | < 2.5 % | 1.066 % (idle, bench_cpu --all; before the meter's histogram rewrite, which removes per-block work) | 2026-11-03 |
| Limiter pluginval / auval / clap-validator (macOS local) | pass | VST3, AU, CLAP PASS | 2026-11-03 |
| Limiter mono in / mono out: +6 dB input gain reads +6 dB; 16-bit dither on silence leaves one TPDF floor | match the stereo path within 0.1 dB; −96.3 ± 1 dBFS | pass (was +12 dB and two floors: review fix) | 2026-11-03 |
| Multiband band count 4 → 5 while a 100 Hz / 1 kHz tone runs | sample step ≤ 1.5× the static output's own | pass (was 7.1× / 5.9×: review fix, 30 ms warm-up + 20 ms crossfade) | 2026-11-03 |
| Convolver: a second spectra swap inside a running fade | monotonic glide, no step above the fade's slope | pass (was a 0.47 step on a DC input: review fix, the swap waits for the fade) | 2026-11-03 |
| Loudness meter: 10 min at −30 dBFS then 1 min 6 dB louder | integrated +1.05 LU; LRA 6 LU | pass (integration no longer stops at 600 s; per-block cost constant: review fix) | 2026-11-03 |
| Mastering chain (LP-EQ → multiband → bus comp → M/S → clipper → limiter) on a synthetic −18 LUFS mix | −14 LUFS ± 0.5; ≤ −1 dBTP + 0.1 | −14.0 LUFS-I; −5.4 dBTP; LRA 0.03; summed CPU 3.88 % of the 8.9 % budget sum (idle bench) | 2026-11-03 |
| Stability fuzz 120 s, all six | no NaN, no stuck gain, pure-gain reference within tolerance | pass (30 cases, MM_FUZZ_SECONDS=120, 2026-11-03 22:50) | 2026-11-03 |
| CPU, idle M2 Max, 100 instances, min of 3 × 3 s (bench_cpu --all) | budgets | lp_eq 0.943, multiband_comp 0.994, bus_comp 0.120, ms_processor 0.135, soft_clipper 0.626, limiter 1.066 % (every other plugin within budget: utility 0.055, channel_eq 0.125, channel_comp 0.274, gate 0.157, saturation 0.731, delay 0.282, reverb 0.827, deesser 0.353, imager 0.179) | 2026-11-03 |
