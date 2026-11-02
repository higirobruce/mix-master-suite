# Review: M1 Core DSP
Commit: 15370ef   Branch: milestone/m1-core-dsp

## What was built
Every shared DSP module the plugins compose, in `core/` (namespace `mm`, header-only, 64-bit coefficient design, float or double processing), each with Catch2 unit tests, Python measurement tests through `mm_render --module`, and the 44.1/48/96/192 kHz matrix:
- Filters: `Biquad` (TDF-II) with RBJ designs and `evaluate()`; Vicanek matched biquads with the analog prototype and a bilinear fallback above ~0.45 fs; TPT `SVF` (8 modes, modulation-safe); `OnePoleLP/HP` (double state), first- and second-order all-pass; windowed-sinc / half-band FIR design and a direct-form `FIRFilter`; Linkwitz–Riley LR2/4/8 crossovers (phase-coherent sum).
- Dynamics: `Detector` (peak / RMS), log-domain `EnvelopeFollower` with 10–90 % attack/release and auto release, `GainComputer` (soft knee, ∞, expander with range), `Lookahead` (delay + O(1) sliding max), `SidechainFilter`; a full feed-forward compressor chain module with ballistics on the gain reduction.
- Smoothing: linear (exact ramp) and exponential modes, double state.
- Delay: `FractionalDelayLine` (Lagrange-3, Thiran-1, slewed time changes) and `TempoSync`.
- Oversampling: `Oversampler` (JUCE equiripple half-band FIR, integer latency, factor 1/2/4/8).
- Nonlinear: tanh / atan / asymmetric / soft clip / hard clip with antiderivatives, first-order ADAA, Jiles–Atherton tape hysteresis (RK4), tube transfer.
- Metering: correlation meter, lock-free FFT spectrum analyser (push from audio, frames on GUI, peak hold).
- 17 render modules in `mm_render` (`--list-modules`), `tools/bench/mm_bench_core` (ns/sample), `docs/bench/core-2026-11-01.json`.

## Exit criteria (spec) → result (measured)
See the "M1 measurements" table in docs/STATE.md for every number. Summary:
| criterion | target | measured | pass |
| --- | --- | --- | --- |
| Filters match analytic response, 4 rates | ±0.1 dB / ±2° | pass for biquad, matched, SVF, one-pole, all-pass, FIR, LR2/4/8 | ✓ |
| Null test, linear modules | < −120 dBFS | float biquad/FIR null < −120 dBFS; block-size independence < −120 dBFS for 9 modules | ✓ |
| Dynamics attack/release | within 10 % | within 10 % on steps (envelope and compressor chain), 4 rates | ✓ |
| Ratio | within 0.1 | within 0.1 for 2/4/8/20/∞ (RMS detector) | ✓ |
| Aliasing with oversampling on, 19 kHz | < −80 dBFS | tanh +6 dB at 8×+ADAA1: pass (4×: −79.7 dBFS at 44.1 k, at the gate); tube 4×: pass; tape 8×: pass; **tanh +18 dB at 4×: −44 dBFS at 48 k (not met, spec question)** | ✓ / ✗ |
| Latency reported (oversampler, lookahead) | exact | exact at every factor and rate | ✓ |
| Sample-rate matrix on every module | 4 rates | every module and every test | ✓ |
| Sanitizers | clean | ASan/UBSan: all C++ tests and the core/plugin Python suites clean (local Debug tree) | ✓ |
| CI | green | run https://github.com/higirobruce/mix-master-suite/actions/runs/41351360069: build-test macos-15 / windows-2022 / ubuntu-22.04 success, sanitizers macos-15 / ubuntu-22.04 success | ✓ |

## Decisions made without asking (link ADRs)
Recorded as rulings in the execution ledger; the architectural ones:
- Matched biquads use a general three-point magnitude match to the RBJ analog prototypes (DC, Nyquist, f0) with impulse-invariant poles; above ~0.45 fs (or when no real solution exists) they fall back to RBJ. The ±0.1 dB test compares each design to its own transfer function; matched-vs-analog is reported (< 0.5 dB vs RBJ > 0.5 dB at 10 kHz / 44.1 k).
- Crossovers run in double (float LR4 reads 7 dB off at −96 dB at 192 kHz); mm_core stays header-only and links `juce::juce_dsp` as an interface.
- Compressor ballistics act on the static gain reduction in the log domain (Giannoulis 2012). On a 1 kHz sine with a peak detector the settled reduction is 13.7 dB instead of 15 and the apparent attack 1.57× (half-cycle ripple); static curves are measured with the RMS detector and times with DC steps, with the tone-burst behaviour recorded for ADR-0004.
- Smoothers keep double state (float one-poles stall below one ulp at 192 kHz); exponential mode designed to 1/1100 of the step at rampMs.
- First-order ADAA; tape loop width maps to Chowdhury's `c`; tape aliasing gated at 8×.
- The oversampler's test gates its own suppression (4× ≥ 30 dB, 8× ≥ 50 dB over 1×) because the plan's +18 dB tanh at 4× cannot reach −80 dBFS (see Questions).

## Known gaps / deferred
- **Spec question (M3):** "alias products below −80 dBFS with oversampling on" at 19 kHz and full drive. With tanh at +18 dB (a near square wave) the 9th/11th harmonics wrap inside the 4× domain; measured worst alias, tanh: 4×+ADAA1 −67/−44/−68 dBFS at 44.1/48/96 k; 8×+ADAA1 −68/−68/−97; at +12 dB 8×+ADAA1 −91/−95/−100; at +6 dB 4×+ADAA1 −80/−83/−112. M3 must choose: define full drive at the level where the gate holds, use 8×, or add second-order ADAA.
- **Risk (M3):** tape hysteresis costs 311 ns/sample at the oversampled rate: at the 8× it needs for the alias gate that is ~24 % of a core for a stereo instance at 48 k (~48 % at 96 k), against a 1 % saturation budget. Needs a cheaper solver (Newton–Raphson / state-trajectory network), vectorisation, or 4× + ADAA for tape mode.
- Direct-form FIR at 255 taps is 388 ns/sample; M5's linear-phase engine uses partitioned FFT convolution as planned.
- Lagrange-3 droops ~1 dB at 0.25 fs for half-sample delays (recorded); Thiran-1 is flat.
- Soft clipper with a quadratic knee aliases more than tanh (−70/−77 dBFS at +6 dB, 4×; −97/−98 at 8×): its gate is met in M5 with 8×.
- True-peak detector, LUFS and LRA metering land in M5 per the roadmap.

## How to try it (exact commands, which DAW, which preset)
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure          # 88 C++ tests
.venv/bin/python -m pytest tests/core -q -n auto    # measurement suite (~700 cases)
.venv/bin/python tools/measure.py --module biquad   # one module's numbers
./build/tools/render/mm_render_artefacts/Release/mm_render --list-modules
./build/tools/bench/mm_bench_core                   # ns/sample per module
```
No new plugin in this milestone; the Utility plugin from M0 is unchanged.

## Review history
A fresh reviewer (whole branch) confirmed every closed form against its source and found 1 Critical / 7 Important / 8 Minor. Fixed on the branch with a failing test first: first-order ADAA now computes its antiderivative difference in double (float left a −46 dBFS peak error on bass; now < −100 dBFS vs an all-double reference); the matched shelf no longer switches to the bilinear design near Nyquist (clamped pole frequency and a continuous knee-coefficient clamp; +12 dB high shelf sweeps to 20 kHz with < 0.5 dB per 1 % step); the spectrum analyser pushes all channels atomically; float-biquad low-frequency deviation is measured and recorded (EQ runs double, ADR-0003); the ratio gate is 0.1 absolute at every ratio; `FractionalDelayLine::Interp` and `Detector::Mode` aliases added and the roadmap signatures updated; tape cost and the 44.1 k alias number corrected here. Minors deferred to the ledger.

## Blocker (resolved 2026-11-01)
The repository was made public, so Actions minutes are free again; CI run 41351360069 is green on every job. Original note: GitHub Actions on the private repo `higirobruce/mix-master-suite` stopped accepting jobs on 2026-11-01 20:54 UTC (spending limit / payment). Until the limit is raised or the repository is made public, the "CI green on three OSes" gate cannot be demonstrated; the loop continues locally (ctest, pytest, ASan, pluginval on macOS) and pushes commits so CI picks them up when restored. macOS runners bill at 10× and Windows at 2×; a cheaper matrix (Linux on every push, macOS/Windows on `main` and `review/*` tags) is an option if you prefer.

## Questions for the reviewer (only if any)
1. Aliasing gate at full drive (see Known gaps): which of the three resolutions do you want for M3?
2. Matched-biquad tolerance: the ±0.1 dB gate is against each design's own transfer function; the matched-vs-analog residual is reported, not gated. Confirm.
3. Compressor timing: the 10 % attack/release gate is measured on DC steps; sine-burst behaviour is recorded. Confirm that reading of "step and tone-burst tests".
