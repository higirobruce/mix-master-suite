# ADR-0002: Parameter model, state and presets

**Status:** Accepted (2026-11-01)

## Context

CLAUDE.md hard rule 2: every continuous parameter is smoothed; every parameter has a unit, range, default and skew in `params.json`, which is the single source for host automation, the GUI and the AI tool schema, generated at build time so drift fails the build. The spec requires host automation, double-click reset, gain-matched bypass, an output trim on every plugin, A/B/C/D slots, undo, presets as `ValueTree` XML shared across formats, and an AI output contract with unit-bearing parameter names (`attack_ms`, not a normalised float).

## Decision

- `plugins/<name>/params.json` is the only hand-written description of a plugin's parameters. Schema: a `plugin` block (`id` `^[a-z][a-z0-9_]*$`, `name`, `family` ∈ {eq, dynamics, colour, time_space, stereo, mastering, utility} (colour added in M3 for the saturation family), `version`, 4-character `code`, `accent` hex colour) and a `params` array. Each param has `id` (same regex, unique), `name`, `type` ∈ {float, int, bool, choice}; float/int require `unit`, `min`, `max`, `default`, `skew` (> 0) and `smoothing_ms` (≥ 0; 0 means stepped/unsmoothed); choice requires `choices` (≥ 2) and an index `default`; optional `group` (default `main`) and `ai` (default true).
- Reserved parameters `bypass` (bool) and `trim_db` (float, −12..+12 dB, 20 ms smoothing, `ai: false`) are appended by the generator to every plugin and may not be declared by hand.
- `tools/gen_params.py` runs at CMake configure/build time (custom command depending on the JSON) and writes `build/generated/<name>/Params.h` and `tool_schema.json`. Nothing generated is committed. `Params.h` provides string constants `mm::plugins::<id>::id::<param>` and a `mm::ParamTable`; plugin code refers to parameters only through these constants, so a renamed knob is a compile error and the AI schema is regenerated from the same file. The schema (`schema_version` `mm-schema-1`) lists only `ai: true` parameters with type, range, unit and description, `additionalProperties: false`; M7 derives the GBNF grammar from it.
- Parameter ids carry their unit as a suffix (`_db`, `_ms`, `_hz`, `_pct`, `_deg`, `_s`) so the AI contract and the host display agree.
- `juce::AudioProcessorValueTreeState` is the parameter store, built from the `ParamTable` (`NormalisableRange` with the JSON skew, unit-aware text conversion). Every float parameter with `smoothing_ms > 0` has an `mm::Smoother<float>` owned by the processor base; targets are refreshed at the start of each block from the APVTS atomics and read per sample or per block by the DSP. Smoothing happens in the parameter's native unit (dB parameters ramp in dB); the DSP reads the smoother at block rate and ramps the derived gain linearly across the block (`applyGainRamp`), so the ramp follows the dB curve at block granularity (64-sample blocks: 15 segments over a 20 ms ramp; a 4096-sample offline block: one linear segment).
- Plugin state (`getStateInformation`) is a `ValueTree` `MM_STATE` with `version` and `pluginId`, holding the APVTS state, `SLOTS` (A/B/C/D parameter snapshots and the active slot), `UI` (scale, theme, drawer state) and a plugin-specific `EXTRA` subtree. Loading ignores unknown parameter ids and leaves missing ones at default; a state with a different `pluginId` is rejected.
- Delta-listen is UI state on the processor (atomic flag), not a parameter, so it is never automated or saved into presets.
- Preset file `.mmpreset` is the XML of a `ValueTree` `PRESET` with `name`, `plugin`, `version` and a `meta` property holding a JSON string (`tags`, `author`, `notes`), and a `PARAMS` child of `PARAM {id, value}` nodes with values in real-world units. Factory presets are compiled into the plugin as binary data; user presets live in `~/Documents/MixMaster/Presets/<pluginId>/`.

## Consequences

- Adding a parameter is one JSON edit plus the DSP that reads it; host layout, GUI attachment, state, presets and the AI schema follow automatically.
- Python is required to configure the build (accepted in ADR-0001).
- Parameter ids are part of the preset and automation contract: renaming one after release needs a migration entry (planned as an optional `aliases` field, not implemented until needed).
- Real-world values in presets keep them readable and format-independent; a changed range still loads because values are clamped on apply.
