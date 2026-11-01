# Project state

Updated: 2026-11-01

## Milestones

| Milestone | Status | Branch | Review tag |
| --- | --- | --- | --- |
| M0 Scaffold | done; CI green (run 41335217507); merged to main | milestone/m0-scaffold | review/m0-scaffold |
| M1 Core DSP | planned (docs/superpowers/plans/2026-11-01-m1-core-dsp.md) | — | — |
| M2 Channel essentials | planned (roadmap) | — | — |
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
| pluginval strictness 10 | pass | VST3 PASS on macOS/Windows/Linux, AU PASS on macOS (CI) | 2026-11-01 |
| clap-validator | pass | 32 passed, 0 failed, 1 warning, 11 skipped | 2026-11-01 |
| auval | pass | AU VALIDATION SUCCEEDED | 2026-11-01 |
| ASan/UBSan (Debug) | no report | ctest 31/31, pytest plugins pass under sanitizers | 2026-11-01 |
| CI matrix macOS/Windows/Linux | green | run 41335217507: 5/5 jobs success | 2026-11-01 |
