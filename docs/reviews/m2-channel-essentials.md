# Review: M2 Channel essentials
Commit: 4c77713   Branch: milestone/m2-channel-essentials

## What was built
Three plugins on the shared base (`plugin_base/PluginProcessorBase`, `gui/PluginEditorBase`), composing the M1 modules:
- **Channel EQ** (`plugins/channel_eq`, 36 parameters): HP/LP at 6/12/18/24/36/48 dB/oct (Butterworth cascades: TPT one-pole + TPT state-variable sections), five bands (bell / low shelf / high shelf) as Vicanek matched biquads in double, digital or analog (proportional) Q, optional 2× oversampling, pre/post spectrum analyser. Every parameter move is a per-sample coefficient interpolation across the block; every switch (band on/off, gain through 0 dB, HP/LP on/off, slope) is a phase-continuous glide (20 ms, or 5 ms per octave with an eased start for cutoffs), never a step. Editor: draggable curve over the analyser (drag = frequency/gain, wheel = Q, hold = solo), one column per band in the strip, Q mode / oversampling / analyser in the drawer. The editor only writes parameters inside host gestures; the curve is designed from parameter values on the message thread.
- **Channel Compressor** (`plugins/channel_comp`, 19 parameters): sidechain HP → peak/RMS detector → soft-knee gain computer → attack/release ballistics on the gain reduction (ADR-0004), feed-forward or feedback (the loop is mapped so the labelled ratio holds; the editor shows the effective ratio when the loop's stability bound caps it), stereo link, mix, manual/auto makeup (ramped per sample, auto capped at +24 dB), external key with silent fallback to the internal key, listen. Editor: GR history with a draggable threshold, transfer curve with the live operating point, GR meter.
- **Gate** (`plugins/gate`, 17 parameters): linked peak key with a 2 ms peak hold, sidechain HP/LP, 0–5 ms lookahead (integer latency applied at block start with a 5 ms tap crossfade, reported to the host), state machine with hysteresis and hold, expander mode through the expander gain computer floored at the range, external key, listen. Editor: history with open/closed shading, mode-aware transfer curve, GR meter with OPEN/CLOSED state.
- **Shared GUI** additions in `gui/`: `CurveDisplay`, `SpectrumDisplay`, `AdvancedDrawer`, `TransferCurveDisplay`, `HistoryDisplay`, `GainReductionMeter`; per-plugin offscreen snapshot tool (`mm_snapshot_<plugin>`, PNGs in `docs/snapshots/`).
- **Base**: `beginBlock()` hook (latency-changing decisions land before the dry copy), smoothers snapped before `prepareDsp`, display names.
- **Tests**: 106 C++ tests (incl. GUI tests for the three editors and factory-preset checks per plugin), ~430 Python measurement cases (`tests/plugins/test_channel_eq.py`, `test_channel_comp.py`, `test_gate.py`), `tests/plugins/test_stability.py` fuzz (every parameter, SR 44.1→96→48 k and block 1…4096 mid-stream; `MM_FUZZ_SECONDS`, nightly 3600 on macOS + Linux), click tests for every switch path. Factory presets: EQ 10, compressor 10, gate 9, utility 8.

## Exit criteria (spec) → result (measured)
Every number is in the "M2 measurements" table of docs/STATE.md.
| criterion | target | measured | pass |
| --- | --- | --- | --- |
| Frequency response (EQ: every band type and slope vs its design, 4 rates; composed response vs independent reference) | ±0.1 dB / ±2° | 120 cases: 0.000 dB / 0.000°; composed reference pass; −3 dB at fc ±0.1 dB at 4 rates | ✓ |
| Null test at flat settings (EQ) / 1:1 (compressor) | < −120 / < −90 dBFS | −200 dBFS / < −90 dBFS at 4 rates | ✓ |
| Attack / release (compressor 1/10/100 ms, 50/300/1000 ms; gate 0.1/1/10 ms, 10/100/1000 ms), DC steps, 4 rates | within 10 % | compressor worst −1.1 %; gate attack 0.091–0.104 ms at 0.1 ms, else exact; hold 50/200/1000 → 51/201/1001 ms | ✓ |
| Ratio (compressor 2/4/8/20/∞ feed-forward; feedback 2/4/8; expander 2/4) | within 0.1 (feedback 0.3) | 2.000 / 4.000 / 8.000 / 20.000; ∞ flat; feedback within 0.3; expander 2.000 / 4.000 | ✓ |
| CPU within budget (EQ 0.5 %, comp 0.4 %, gate 0.3 %; 100 instances, 48 k / 64) | ≤ budget | local: EQ 0.122, comp 0.260, gate 0.167 %; CI macOS 0.090 / 0.192 / 0.121, Linux 0.129 / 0.288 / 0.164, Windows 0.138 / 0.249 / 0.168 % | ✓ |
| Hysteresis prevents chatter | ≤ 1 transition per AM cycle | 0 at 3 dB (15 at 0 dB, recorded) | ✓ |
| Lookahead latency reported == measured; onset preserved | exact; ≥ 0.9 | 240 == 240; onset ratio 1.00 (0.30 without) | ✓ |
| No clicks on parameter moves and switches (Review Focus 1) | 1 ms envelope step < 0.5 dB (sweeps) / glides never step | band sweep 0.14 dB; HP/LP cutoff sweeps ≤ 0.1 dB at every slope; band gain→0 and on/off ≤ 1.5 dB per ms (20 ms glide); HP on/off/slope ≤ 0.8 dB per ms; comp mix/auto-makeup ≤ 2.5 / 1.0; gate lookahead change: no sample jump | ✓ |
| Stability fuzz (2 min per plugin on push, 1 h nightly) | no NaN, bounded | pass ×4 (peaks EQ +49.6 with five +18 dB bells, comp +19.7, gate +11.3, utility +24.9 dBFS) | ✓ |
| pluginval strictness 10, auval, clap-validator | pass | EQ / comp / gate: VST3, AU, CLAP PASS (macOS local; CI runs every plugin on three OSes) | ✓ |
| Sanitizers | clean | CI sanitizer jobs (macOS + Linux) green | ✓ |
| CI matrix green | green | run 41386855649, five jobs green | ✓ |

## Decisions made without asking (link ADRs)
ADR-0003 (EQ bands and analog Q), ADR-0004 (compressor topology), ADR-0005 (gate hysteresis, hold, lookahead), each with a dated "Amendments" section from the review fix pass. Rulings recorded in the execution ledger:
- EQ: the 36 dB/oct slope is three Butterworth biquads (the plan's one-pole + three biquads was 7th order); coefficient changes interpolate per sample across the block; the oversampling toggle is a configuration change (state reset, no dual bank); the 2× gate asserts the 2× path within 0.1 dB of the analog prototype to 0.4 fs; ramp smoothness for filters is measured on 1 ms RMS envelopes.
- Compressor: external key falls back to the internal key when the host leaves the sidechain bus disabled; the feedback loop is mapped to the labelled ratio and capped by its stability bound (shown in the footer); the sample-rate-change check settles 0.6 s for dynamics.
- Gate: a fixed 2 ms key peak hold (without it hysteresis never engaged: the raw |x| touches zero every half cycle); linked key, peak detector only; expander hysteresis gates the switch between 0 dB and the static curve; lookahead is the one unsmoothed continuous parameter (integer latency, 5 ms tap crossfade).
- Fuzz: automation every 50 ms applied at block boundaries (how hosts deliver it); peak bound = input + maximum static gain + 24 dB (an EQ with five +18 dB bells legitimately reaches +78 dBFS; instability reaches float range or NaN); pure-gain jump check on a separate sine render.
- Base: smoothers snap before `prepareDsp`; `beginBlock()` added so latency changes and the dry path move together.

## Known gaps / deferred
- Deferred minors from the whole-branch review (ledger): mono→stereo instances filter the second (host) channel before the base overwrites it (harmless extra work); the spectrum analyser re-prepare could race a GUI frame if a host calls `prepareToPlay` off the message thread; solo is implemented by writing the band-enable parameters (visible to automation/undo while held); bass material with hold = 0 relies on hold below 250 Hz (ADR-0005); the oversampling toggle clicks (−37 dBFS; configuration change, ADR-0003); the dynamics sample-rate-change tolerance (−60 dBFS after 0.6 s) is looser than what is measured (−126 dBFS).
- Feedback compression above ~22:1 at a 1 ms attack (or ~2.7:1 at 0.1 ms) is capped by the loop's stability bound; the footer shows the effective ratio.
- An HP/LP switch at 48 dB/oct on material sitting exactly at the cutoff still shows up to +1 dB of resonant release during the glide (eased 5 ms-per-octave glide; was +5.5 dB).
- Open from M1 for M3: the alias gate at full drive (8× / ADAA2 / full-drive definition) and the tape hysteresis cost.

## How to try it (exact commands, which DAW, which preset)
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure                      # 106 C++ tests
.venv/bin/python -m pytest tests/plugins -q -n auto -k "not cpu_budget"
.venv/bin/python -m pytest tests/plugins -q -k cpu_budget      # serial: timing
MM_FUZZ_SECONDS=120 .venv/bin/python -m pytest tests/plugins/test_stability.py -q -n 4
tools/run_pluginval.sh channel_eq && tools/run_pluginval.sh channel_comp && tools/run_pluginval.sh gate
.venv/bin/python tools/bench_cpu.py --all --instances 100
./build/tools/snapshot/mm_snapshot_channel_eq docs/snapshots 1.0   # editor PNGs, dark + light
```
DAW: load the VST3/AU from `build/plugins/<name>/<name>_artefacts/Release/` (CLAP next to them). Presets to start with: EQ "Vocal · Presence lift" (drag the 3.5 kHz node, wheel for Q, hold a node to solo it), Compressor "Vocal · Smooth" (drag the threshold line on the history; switch topology to Feedback and watch the footer), Gate "Snare · Tight" (open/closed shading on the history; raise lookahead to 5 ms and check the host's latency readout).

## Review history
A fresh whole-branch reviewer found 2 Critical / 6 Important / 7 Minor. Fixed on the branch, each with a failing test first: the EQ editor no longer calls into audio-thread design code (static response from parameter values, host gestures around drags; GUI test asserts the DSP design is untouched by a drag); HP/LP cutoff moves no longer click at 6/18 dB/oct (the one-pole was re-prepared, i.e. reset, on every redesign) and switches glide instead of stepping (sections on the TPT SVF, eased per-octave glide; +5.5 dB → +1 dB worst case); bands finish their ramp to 0 dB before leaving the chain; the feedback compressor realises its labelled ratio with the detector pre-makeup; mix and auto makeup ramp per sample with the auto term capped; the gate's lookahead is unsmoothed and crossfades its delay tap; the four weakened/missing test halves were restored (band sweep envelope step, ∞:1 at −60 dB finite/no DC, loud external key drives reduction (comp + gate), HP/LP −3 dB at fc ±0.1 dB at four rates). Minors deferred to the ledger and listed above. CI then measured the compressor at 0.50 % and the gate at 0.31 % on the Windows runner (the spec's "mid-range laptop"): per-sample pow/log10 were replaced by fast log2/exp2 conversions within 1e-4 dB (tested), a fully linked compressor computes one gain, settled envelopes land exactly so steady-state gains are exact constants, and the sliding maximum lost its per-sample integer modulo; the gate also skips idle sidechain filters and the zero-lookahead delay read. Windows now reads comp 0.249 %, gate 0.168 %.

## Questions for the reviewer (only if any)
1. Feedback topology: the loop realises the labelled ratio up to its stability bound and the footer discloses the cap. Alternative: hide ratios above the bound in feedback mode. Preference?
2. HP/LP switch glides take 5 ms per octave (45 ms for an HP at 1 kHz). Acceptable, or do you want a plain 20 ms with the residual bump?
