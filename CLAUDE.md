# Mix & Master Plugin Suite — Claude Code starter

You are building a professional mixing and mastering plugin suite (16 DSP plugins + an offline AI prompt layer) from the spec in `docs/SPEC.md`. That file is the source of truth; when this file and the spec disagree, the spec wins. Read it fully before the first action.

Work in autonomous loops: pick the next milestone, build it, verify it against the exit criteria, self-review, fix, repeat. Stop only when a milestone is ready for human review or you hit a blocker you cannot resolve. Do not ask the user questions you can answer from the spec, the codebase, or by running something.

## Stack (fixed)

- C++20, JUCE 8, CMake ≥ 3.25, exporting VST3, AU, AAX (stub if SDK absent), CLAP, and Standalone.
- Test harness in Python 3.11: numpy, scipy, pyloudnorm, soundfile; `pluginval` for host validation.
- AI layer: embedded `llama.cpp` (GGUF, grammar-constrained JSON), ONNX Runtime statically linked. Everything runs offline; no network calls at plugin runtime, ever.
- Fine-tuning pipeline: Python, MLX (local Mac) and PyTorch + PEFT/QLoRA (rented GPU), exporting GGUF Q4_K_M.
- CI: GitHub Actions, macOS + Windows + Linux matrix, runs the measurement suite and pluginval on every push.

## Repo layout (create exactly this)

```
/core            shared DSP library (filters, dynamics, oversampling, nonlinear, delay, metering, smoothing)
/plugins/<name>  one folder per plugin: Processor, Editor, params.json, presets/
/gui             shared look-and-feel, knobs, meters, spectrum, header/footer chrome
/ai              feature extractor, schema generator, llama.cpp host, prompt templates
/ml              data/, train/, eval/, export/ for the fine-tuned model
/tests           python measurement suite + fixtures; C++ unit tests (Catch2)
/tools           pluginval runner, CPU benchmark, installer scripts
/docs            SPEC.md, ADRs (docs/adr/NNNN-title.md), REVIEW.md reports
```

## Hard rules (never violate)

1. Audio thread: no allocation, no locks, no I/O, no logging, no exceptions. All buffers sized in `prepareToPlay`. Run with `-fsanitize=address,undefined` in debug; a sanitizer hit is a failed milestone.
2. Every continuous parameter is smoothed; every parameter has a unit, range, default, and skew in `params.json`. The plugin's `params.json` is the single source for parameter layout, host automation, the GUI, and the AI tool schema (generated at build time; drift fails the build).
3. Sample-rate independence: every DSP module has a test at 44.1k, 48k, 96k and 192k. Nonlinear stages always run oversampled.
4. Latency is always reported to the host, including oversampling and lookahead.
5. Tests before done: a milestone is not complete until its spec exit criteria pass in CI on all three OSes. Do not weaken a test to pass it; fix the code or raise it in REVIEW.md as a spec question.
6. Dependencies pinned to exact versions, vendored or fetched by CMake FetchContent with a hash. Nothing fetched at plugin runtime.
7. Commit small and often. Conventional commits (`feat(core): …`, `test(limiter): …`). Never commit a broken build to `main`; use `milestone/<name>` branches and merge when the gate passes.
8. Architecture decisions get an ADR in `docs/adr/` before the code lands. One page, status, context, decision, consequences.

## The loop

Run this cycle until a milestone is ready for review:

1. **Orient** — read `docs/STATE.md` (current milestone, what passed, what's open). If missing, create it from the Milestones list below.
2. **Plan** — write the milestone's task list into `docs/STATE.md` with exit criteria copied verbatim from the spec. Keep tasks under ~2 hours each.
3. **Build** — implement the next task. Prefer composing existing `/core` modules over writing new DSP.
4. **Verify** — run the relevant tests: unit, measurement suite, pluginval, sanitizers, CPU benchmark. Record numbers in `docs/STATE.md`.
5. **Self-review** — as a second engineer, read the diff for: audio-thread safety, denormals/NaN handling, parameter edge cases (min, max, default, rapid changes), sample-rate and buffer-size changes mid-stream, mono/stereo configs, state save/restore round-trip. Fix what you find before moving on.
6. **Iterate** — back to step 3 until every exit criterion for the milestone passes.
7. **Report** — write `docs/REVIEW.md` (format below), tag the commit `review/<milestone>`, and stop with the line `READY FOR REVIEW: <milestone>`.

Stop early and write `BLOCKED: <reason>` in REVIEW.md only when: a required SDK or licence is missing, the spec is contradictory in a way that changes architecture, or a test cannot pass without weakening the spec. Everything else, decide and document in an ADR.

## Milestones (in order; exit criteria are in the spec, summarised here)

- **M0 Scaffold** — repo layout, CMake, CI matrix green, Utility plugin builds in all formats, pluginval level 10 passes on macOS/Windows/Linux. ADR-0001 build system, ADR-0002 parameter model.
- **M1 Core DSP** — filters (biquad, matched, FIR, Linkwitz–Riley), dynamics core, smoothing, metering FIFO, delay line, oversampling, nonlinear/ADAA. Each with unit tests, frequency/step-response measurement tests, sample-rate matrix. Null test residual < −120 dBFS for linear modules.
- **M2 Channel essentials** — Channel EQ, Channel Compressor, Gate. Frequency response within ±0.1 dB / ±2°; attack/release within 10%; ratio within 0.1; CPU within budget table.
- **M3 Colour and space** — Saturation (aliasing < −80 dBFS at 19 kHz, 4× OS), Delay, Reverb (FDN + Dattorro plate). Reverb decay time measured within 10% of labelled RT60.
- **M4 Stereo and vocal** — De-esser, Stereo Imager. Mono-compatibility and correlation tests.
- **M5 Mastering** — Linear-phase FIR engine, Linear-phase EQ, Multiband comp, Bus comp, M/S processor, Soft clipper, True-peak limiter. EBU Tech 3341/3342 vectors within ±0.1 LU and ±0.3 dBTP; limiter never exceeds ceiling by more than 0.1 dBTP on the test set.
- **M6 Delivery** — Loudness meter + dither, preset libraries, shared GUI chrome on every plugin, installer (Full/Lite), licensing stub, docs.
- **M7 AI layer** — feature extractor, schema generator, llama.cpp host (shared singleton, worker thread), prompt bar + proposal diff UI, `/ml` pipeline (data tooling, MLX and PyTorch training scripts, eval suite implementing the spec's release gate, GGUF export), Full installer bundling the model. The first model candidate ships only when it beats the rules-based mapper and the 7B zero-shot baseline on the eval suite.

Deliver M0 through M7 in order. Within M2–M5 you may reorder plugins if a dependency makes it cheaper.

## GUI

Follow the UI/UX section of the spec and the mockups described there: shared header (name, presets, A/B/C/D, undo, delta, gain-matched bypass, settings), dominant main display, control strip, metering footer. Vector only, resizable 50–200%, dark default with light theme, one accent colour per plugin family. Build the shared chrome once in `/gui` and never fork it per plugin.

## Fine-tuning data (M7)

Draft prompt → parameter JSONL examples yourself, 100 per plugin per batch, in the contract format from the spec, with a `source_class`, `prompt`, `target_params`, `rationale`, and a `stem_id` placeholder for the engineer to attach a real feature block. Put the first batch in `ml/data/drafts/` and list it in REVIEW.md for engineer verification. Never fabricate feature blocks; the extractor computes those from real stems.

## REVIEW.md format

```
# Review: <milestone>
Commit: <sha>   Branch: milestone/<name>
## What was built
## Exit criteria (spec) → result (measured)
| criterion | target | measured | pass |
## Decisions made without asking (link ADRs)
## Known gaps / deferred
## How to try it (exact commands, which DAW, which preset)
## Questions for the reviewer (only if any)
```

## Commands

```
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
python -m pytest tests/ -q
python tools/measure.py --plugin <name> --all
tools/run_pluginval.sh <plugin>
python tools/bench_cpu.py --plugin <name> --instances 100
```

Keep these working; if you change them, update this file in the same commit.

---

## Kick-off prompt (paste into Claude Code to start the loop)

```
Read CLAUDE.md and docs/SPEC.md in full. Create docs/STATE.md from the Milestones list, then start the loop at M0. Run autonomously: plan, build, verify, self-review, iterate. Do not stop to ask questions that the spec, the code, or a test can answer. Stop only when you can write "READY FOR REVIEW: M0" with a complete docs/REVIEW.md, or "BLOCKED: <reason>". Then wait.
```

After each review, continue with:

```
Review feedback is in docs/REVIEW.md under "Reviewer notes". Address every note, then continue the loop to the next milestone.
```
