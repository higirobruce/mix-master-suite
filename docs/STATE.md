# Project state

Updated: 2026-11-01

## Milestones

| Milestone | Status | Branch | Review tag |
| --- | --- | --- | --- |
| M0 Scaffold | in progress | milestone/m0-scaffold | — |
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

- [ ] Task 0 Toolchain, repository, branch
- [ ] Task 1 ADR-0001 build system, ADR-0002 parameter model
- [ ] Task 2 Root CMake, dependency pins, warnings, sanitizers
- [ ] Task 3 Parameter generator tools/gen_params.py
- [ ] Task 4 First core modules + Catch2 tests
- [ ] Task 5 mm_plugin_base
- [ ] Task 6 Utility plugin
- [ ] Task 7 mm_render offline host
- [ ] Task 8 Python harness and Utility measurement suite
- [ ] Task 9 mm_gui shared chrome and Utility editor
- [ ] Task 10 pluginval runner, CPU benchmark, CI matrix
- [ ] Task 11 Factory presets, STATE, REVIEW, tag

## Measurements

| Criterion | Target | Measured | Date |
| --- | --- | --- | --- |
