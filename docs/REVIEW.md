# Review: M4 Stereo and vocal
Commit: <sha>   Branch: milestone/m4-stereo-and-vocal

## What was built
- **De-esser** (`plugins/deesser`, 9 parameters, ADR-0009): a linked peak detector on the sibilance band (Band mode: TPT SVF band-pass at the labelled width in octaves; Split mode: the high band of an LR4 at the frequency), a 0–3 ms lookahead on the detector with the dry path delayed to match (latency reported), a 0.5 / 60 ms envelope in dB with the reduction clamped to `range_db`. Band mode cuts with a dynamic bell whose Q is corrected for the TPT peak's depth-dependent bandwidth (`Q / A`), updated every 8 samples on a running counter so the result is block-size independent; Split mode scales the high band of a second LR4 on the delayed dry. Listen outputs dry − wet. Editor: scrolling peak history with sibilant frames shaded and the reduction drawn from the top, GR meter, Listen toggle, strip frequency / threshold / range / width / mode / lookahead.
- **Stereo Imager** (`plugins/imager`, 10 parameters, ADR-0010 + Amendment): M/S encode; the side is split into three bands by two 64-bit LR4 crossovers and each band scaled by its width (0–200 %); the mid and the side's low band go through the matching second-order all-passes (an LR4's low + high is exactly that all-pass) so width 100 is the all-passed input and L + R never changes with width; mono-below removes the side under its frequency with a third LR4 (unsmoothed parameter, engaged by a 20 ms crossfade, glided at 5 ms per octave while on); constant-power pan; output gain; zero latency. Correlation meter (300 ms) and a stereo scope ring for the editor. Editor: goniometer (mono vertical, newest samples brightest) with a −1…+1 correlation bar, a small output spectrum carrying the mono-below line, strip width low / crossover / width mid / crossover / width high / pan / output, mono-below in the drawer.
- GUI: `SibilanceDisplay`, `Goniometer`, `SpectrumDisplay::setMarkerHz`. Presets: de-esser 10, imager 11. Fuzz coverage for both (the imager's pure-gain envelope is judged against its LR4 all-pass reference).

## Exit criteria (spec) → result (measured)
Every number is in the "M4 measurements" table of docs/STATE.md.
| criterion | target | measured | pass |
| --- | --- | --- | --- |
| De-esser reduces only the sibilance band (1 kHz untouched), Band and Split, 4 rates | 1 kHz ±0.05 dB; band < −6 dB | 1 kHz −0.026 … −0.029 dB; band −17.7 (Band) / −10.6 … −11.2 dB (Split) | ✓ |
| De-esser threshold / range cap / Split sums flat / Listen = dry − wet | 0 dB below threshold; 6 ± 0.2; ±0.05; null | pass / 5.98 and 5.91 dB / pass / pass | ✓ |
| De-esser lookahead latency reported == measured (0 / 1 / 3 ms, 4 rates); live change click-free | exact; no jump > 1.5× | pass / pass | ✓ |
| Imager mono compatibility: L + R unchanged for width 0 / 50 / 150 / 200 per band; width 0 → L = R | ±0.1 dB; correlation 1 | pass / pass | ✓ |
| Imager width 100 = all-passed input, 4 rates; width 200 = +6.02 dB per band | null < −100 dBFS, flat ±0.02; ±0.1 | pass at 44.1 / 48 / 96 / 192 k; +5.98 / +5.97 / +6.01 dB | ✓ |
| Imager mono-below / pan laws / mono input / correlation meter | side < −60 dB at 2.9 oct; constant power; = all-passed input; ±0.02 | −70.0 dB / pass / pass / pass | ✓ |
| Click-free automation: width sweep, mono-below on/drag/off, crossover drags, de-esser lookahead | no sample jump above the static output's own slope (1.1×; 2× for mono-below and lookahead) | pass | ✓ |
| CPU within budget (0.4 / 0.4 %) | ≤ budget | local: de-esser 0.358 %, imager 0.172 %; CI: see run below | ✓ |
| Stability fuzz 120 s, pluginval / auval / clap-validator, base checks | pass | pass for both (macOS local; CI three OSes) | ✓ |
| CI matrix + sanitizers | green | run <id> | <pending> |

## Decisions made without asking (link ADRs)
ADR-0009 (de-esser topology), ADR-0010 + Amendment (imager topology; all-pass legs as single biquads, 64-bit crossovers, crossfaded mono-below engagement). Ledger rulings worth knowing:
- **De-esser lookahead unsmoothed** (integer latency applied at block start with a 5 ms tap crossfade, as the gate).
- **Bell Q corrected for depth** (the TPT peak's cut bandwidth scales with A; the bell takes Q / A so the labelled width holds); Split-mode tests put the sibilance above the crossover (a tone at the crossover is half in each band by design).
- **Imager width 100 is the all-passed input, not a sample null**: the spec names Linkwitz–Riley; linear-phase crossovers arrive with M5. Crossovers run in double (float left the 192 k null at −83 dBFS) and the all-pass legs are single rbj all-pass biquads (exact LR4 low + high identity): 16 biquads per sample instead of 28, 0.17 %.
- **`mono_below_hz` unsmoothed**: engagement by a 20 ms crossfade with the filters started at the target cutoff, 5 ms/octave glide while on. A smoothed cutoff ramp from 0 Hz drove the TDF-II LR4s through a 5 → 200 Hz sweep in 20 ms and clicked (0.17 peak on a −12 dBFS tone).
- **Test geometry**: band tones sit ≥ 2 octaves inside their band (the LR4 edge is −0.22 dB at 1.3 octaves, so a 60 Hz tone against a 150 Hz crossover reads +5.9 dB at width 200); mono-below is gated 2.9 octaves below its frequency (−42 dB at 1.7 octaves is recorded); click tests are bounded by the static output's own slope at either end of a drag; the correlation meter gets 2 s per pair (exponential 300 ms window keeps 19 % of the previous pair after 0.5 s).

## Known gaps / deferred
- Deferred minors from the whole-branch review (ledger): the goniometer scope ring is read by the editor while the audio thread writes it (aligned floats, no tearing; a cursor FIFO would also stop redrawing ~2.5× duplicated samples); mono-below between 0 and 5 Hz is off while the knob shows a value; on a mono output bus the pan lowers the sum by up to 3 dB and the correlation meter holds its last value.
- The de-esser's budget is tight on the reference machine (0.37 % of 0.40 %); CI runners are judged through the calibrated limit.
- Open from M3: alias gate at full drive, plate decay floor, static tape, reference machine for the CPU budgets (docs/reviews/m3-colour-and-space.md).

## How to try it (exact commands, which DAW, which preset)
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
.venv/bin/python -m pytest tests/plugins -q -n auto -k "not cpu_budget"
.venv/bin/python -m pytest tests/plugins -q -k cpu_budget
MM_FUZZ_SECONDS=120 .venv/bin/python -m pytest tests/plugins/test_stability.py -q -n 4
tools/run_pluginval.sh deesser && tools/run_pluginval.sh imager
.venv/bin/python tools/bench_cpu.py --all --instances 100
./build/tools/snapshot/mm_snapshot_deesser docs/snapshots 1.0 && ./build/tools/snapshot/mm_snapshot_imager docs/snapshots 1.0
```
DAW: load from `build/plugins/<name>/<name>_artefacts/Release/`. Start with De-esser "Vocal · Firm 6 kHz" on a vocal (engage Listen to hear what is removed; switch to Split and compare), Imager "Mix · Wide top, mono below 120" on a mix (watch the goniometer spread with the width knobs and the correlation bar; open the drawer and drag mono-below along the spectrum line).

## Review history
A fresh whole-branch reviewer (opus) found 1 Critical / 5 Important / 6 Minor, each confirmed by rendering audio. Fixed on the branch with a failing test first: a mono input on a stereo bus ran every de-esser filter twice per sample (0.8 dB of the 18 dB reduction survived); Split-mode Listen output dry − allpass(dry) instead of the removed band; a lookahead change landing inside the 5 ms tap crossfade dropped the half-faded tap (de-esser and, by the same pattern, the gate); Band/Split and Listen were hard switches (now 10 ms crossfades); the imager's mono-below glide stepped once per block (clicked at 512 and 2048); pan was a balance control that lost a hard-panned source (now a constant-power stereo rotation); mono-below faded back in for 20 ms after every reset. Suite after the fix pass: ctest 100 %, 62 plugin tests, CPU gates, 120 s fuzz, pluginval all green. Minors deferred to the ledger and listed above.

## Questions for the reviewer (only if any)
1. **Imager phase.** Width 100 is the LR4 all-passed input (magnitude-flat, L/R coherent, L + R preserved) rather than a sample null. Keep minimum-phase crossovers (zero latency), or switch the imager to M5's linear-phase FIR crossovers when they land (latency ~20 ms)?
2. **Mono-below slope.** LR4 (24 dB/oct) leaves −42 dB of side 1.7 octaves below the mono frequency; the gate is measured 2.9 octaves below (the ADR's original "< −60 dB below f" was written without a distance). Acceptable for "mono below", or do you want LR8 (48 dB/oct, double the filter cost) on the side?
3. **Pan as rotation.** The imager's pan is a ±45° constant-power stereo rotation (ADR-0010 amendment 2): a mono source follows the usual sin/cos law, stereo content moves rather than losing a channel, and content already at the far edge rotates into anti-phase at full pan. Keep, or do you prefer a balance control under that name?
