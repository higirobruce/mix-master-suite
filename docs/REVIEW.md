# Review: M0 Scaffold
Commit: 8b2b795   Branch: milestone/m0-scaffold

## What was built
- Repo layout per CLAUDE.md (core, plugin_base, gui, plugins/utility, tools, tests, docs/adr, docs/superpowers/plans).
- CMake ≥ 3.25 + Ninja build with SHA256-pinned JUCE 8.0.16 and Catch2 3.8.1 tarballs and SHA-pinned clap-juce-extensions (ADR-0001). Formats: VST3, AU, CLAP, Standalone. AAX stubbed (no SDK).
- `tools/gen_params.py`: `params.json` → generated `Params.h` (string ids, ParamTable) and `tool_schema.json` (AI schema, `mm-schema-1`) at configure/build time; reserved `bypass` / `trim_db` (ADR-0002).
- `mm_core` first modules: Smoother (linear), IntegerDelay, OnePoleHP (double state), FirstOrderAllPass, MeterFifo (SPSC), PeakMeter, RmsMeter, DbGainCache, ScopedNoDenormals.
- `mm_plugin_base`: APVTS from the ParamTable, per-parameter smoothing, gain-matched bypass (param and host bypass through `getBypassParameter`), delta listen, output trim, latency-aligned dry path, NaN guard, metering FIFO, A/B/C/D slots, undo, presets (`.mmpreset` ValueTree XML + JSON meta), state save/load with forward compatibility, bus layouts mono/stereo/mono→stereo.
- `mm_gui`: Theme (dark/light, family accents), MMLookAndFeel (Inter, flat-arc knobs), Knob, Toggle, Selector, MeterBar, HeaderBar (name, presets, A/B/C/D, undo/redo, delta, bypass, settings), Footer (in/out meters, latency, trim), PluginEditorBase (50–200 % with fixed aspect, 60 Hz meters, theme + scale persisted in plugin state).
- Utility plugin: gain, polarity L/R, swap, mono sum, DC block (10 Hz), phase rotation (first-order all-pass, −θ at 100 Hz), mutes; editor with large stereo meters; six factory presets.
- `mm_render` offline host (VST3/AU + core modules, JSON jobs, automation, block/sample-rate schedules, state round trip, host bypass, `--bench`, `--dump-params`), Python harness, Utility measurement suite, shared base checks, `tools/measure.py`, `tools/bench_cpu.py`, `tools/run_pluginval.sh` (pinned pluginval 1.0.4 + clap-validator 0.4.1, sha256-verified), `tools/snapshot` (offscreen editor PNGs), `tools/bench` (in-process processor cost).
- CI: GitHub Actions matrix macos-14 / windows-2022 / ubuntu-22.04 (build, ctest, pytest, pluginval, clap-validator, bench) + sanitizer job (macOS, Linux). Nightly placeholder for the M2 long fuzz.

## Exit criteria (spec) → result (measured)
| criterion | target | measured | pass |
| --- | --- | --- | --- |
| Repo layout, CMake | as CLAUDE.md | present; `cmake -B build -G Ninja && cmake --build build` | ✓ |
| Utility builds in all formats | VST3, AU, AAX, CLAP (+Standalone) | VST3, AU, CLAP, Standalone built; AAX stubbed (no SDK) | ✓ / AAX gap |
| pluginval level 10 | pass on macOS/Windows/Linux | macOS local: VST3 PASS, AU PASS; Windows/Linux: CI run 41328502710 (see STATE.md for the final status) | pending CI |
| CI matrix green | green | run 41328502710 in progress at the time of writing; the tag is pushed only when green | pending CI |
| ADR-0001, ADR-0002 | written before code | docs/adr/0001-build-system.md, 0002-parameter-model.md | ✓ |
| Null test (utility) | < −120 dBFS | −200 dBFS at 44.1/48/96/192 k | ✓ |
| CPU (utility) | < 0.1 % / instance | 0.055–0.058 % hosted, 100 instances | ✓ |
| Sanitizers | no report | ASan/UBSan clean on ctest and plugin suite | ✓ |

## Decisions made without asking (link ADRs)
- ADR-0001: tarball + SHA256 pins for JUCE/Catch2 (a full JUCE git clone took > 30 min on this network and was killed); Universal binary opt-in locally, ON in CI; hidden symbol visibility for all our targets (a sanitized bundle otherwise coalesced weak JUCE symbols with the host's).
- ADR-0002: reserved `bypass`/`trim_db`; native-unit smoothing processed at block rate with linear gain ramps; delta listen as UI state; preset format.
- Phase rotation defined as a first-order all-pass with −θ at 100 Hz (θ 0–179°); DC block as a 10 Hz TPT one-pole with double state.
- Gain match uses a 400 ms RMS on channel 0 until the BS.1770 meter lands in M5.
- Inter (OFL) bundled as the UI typeface; readouts right-aligned in fixed boxes instead of OpenType tabular figures.
- clap-validator added next to pluginval for the CLAP format (pluginval does not load CLAP).
- `setStateInformation` requests a host parameter rescan (required by CLAP hosts).
- The AU is installed temporarily into `~/Library/Audio/Plug-Ins/Components` for validation and removed afterwards (`MM_KEEP_AU=1` keeps it).
- Milestone gates do not wait for sign-off (user instruction 2026-11-01); reviewer notes are collected afterwards.

## Known gaps / deferred
- AAX: no SDK available; format skipped at configure. Enable with `-DMM_AAX_SDK_PATH=…`.
- Preset browser overlay (search, tags, favourites) is a popup menu until M6; MIDI learn / macros / accessibility labels beyond parameter names deferred to M6.
- The 1-hour parameter fuzz lands with M2 (nightly placeholder exists).
- Listening gate not applicable to Utility.
- Windows/Linux pluginval results come from CI (no local machines).

## How to try it (exact commands, which DAW, which preset)
```
brew install cmake ninja            # macOS
uv venv --python 3.11 .venv && uv pip install --python .venv/bin/python -r tests/requirements.txt
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
.venv/bin/python -m pytest tests -q -n auto
.venv/bin/python tools/measure.py --plugin utility --all
tools/run_pluginval.sh utility
.venv/bin/python tools/bench_cpu.py --plugin utility --instances 100
./build/tools/snapshot/mm_snapshot shots 1.0        # editor PNGs, dark + light
open build/plugins/utility/utility_artefacts/Release/Standalone/Utility.app
```
DAW: copy `build/plugins/utility/utility_artefacts/Release/VST3/Utility.vst3` to `~/Library/Audio/Plug-Ins/VST3` (or run `MM_KEEP_AU=1 tools/run_pluginval.sh utility` to leave the AU installed), load Utility in Reaper or Logic, try preset "Phase Rotate 90" on a kick and "Mono Check" on the mix bus.

## Questions for the reviewer (only if any)
1. GitHub repo `higirobruce/mix-master-suite` (private) was created for CI. Keep, or move to an org?
2. Is an AAX SDK coming? Until then AAX stays a documented gap.
3. M1 filter tolerance: ±0.1 dB / ±2° is tested against each design's own discrete transfer function; the matched-vs-analog gap will be reported separately. Confirm this reading of the spec.
