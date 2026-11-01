# Project state

Updated: 2026-11-01

## Milestones

| Milestone | Status | Branch | Review tag |
| --- | --- | --- | --- |
| M0 Scaffold | built; CI run 41328502710 pending; review tag on green | milestone/m0-scaffold | review/m0-scaffold (after CI) |
| M1 Core DSP | done; CI green (run 41351360069); merged to main | milestone/m1-core-dsp | review/m1-core-dsp |
| M2 Channel essentials | in progress (docs/superpowers/plans/2026-11-01-m2-channel-essentials.md) | milestone/m2-channel-essentials | — |
| M3 Colour and space | planned (roadmap) | — | — |
| M4 Stereo and vocal | planned (roadmap) | — | — |
| M5 Mastering | planned (roadmap) | — | — |
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
