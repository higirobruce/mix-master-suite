# Mix & Master Plugin Suite — Spec

Oct 31, 2026 · @Bruce

## Overview

A complete mix-and-master toolkit is 16 plugins: 9 for mixing, 7 for mastering, all built on one shared C++/JUCE codebase and shipped as VST3, AU, AAX and CLAP (macOS, Windows, Linux).

The suite is scoped to what a professional engineer touches on every song. Anything beyond this (pitch correction, convolution reverb, restoration) is a later phase and not a blocker for finishing a release-quality record.

Design principles:

- One DSP core library shared by every plugin; each plugin is a thin wrapper of parameters + GUI around core modules.
- Zero allocation, locks or I/O on the audio thread; all state pre-allocated in `prepareToPlay`.
- Sample-rate independent (44.1 kHz to 192 kHz) with internal oversampling where nonlinearities exist.
- Every parameter host-automatable, smoothed, with sensible default and a double-click reset.
- Gain-matched bypass and an output trim on every plugin so A/B comparisons are honest.

## Shared foundation

Build these modules once; every plugin composes them. This is roughly 40% of the total effort and should be finished before the first plugin ships.

| Module | What it provides | Key technical choices |
| --- | --- | --- |
| Plugin framework | Project template, parameter layout, state save/load, preset system, host automation, bypass | JUCE 8, `AudioProcessorValueTreeState`, CMake build, Pamplejuce-style CI |
| Filter library | Biquads (peak, shelf, HP/LP, notch), matched-response “analog” EQ (Vicanek/Orfanidis), linear-phase FIR, Linkwitz–Riley crossovers | Double precision, TPT/SVF form for stability under modulation |
| Dynamics core | Peak/RMS/true-peak detectors, attack/release envelopes, soft knee, program-dependent release, lookahead buffer, sidechain HP filter | Log-domain gain computer; detector runs at oversampled rate for limiter |
| Oversampling | 2×/4×/8× polyphase or FIR half-band with latency reporting | Toggle per plugin; off = zero latency, on = reported to host |
| Nonlinear library | Tanh/arctan/asymmetric waveshapers, tape hysteresis, tube transfer curves, ADAA (anti-derivative anti-aliasing) | Always paired with oversampling |
| Delay line | Fractional-sample interpolated delay, tempo sync, feedback path with filter insert | Lagrange or Thiran interpolation |
| Metering | Peak, RMS, LUFS-M/S/I (ITU BS.1770-4), true-peak (4× oversampled), correlation, spectrum (FFT 4096–8192) | Metering on a lock-free FIFO to the GUI thread |
| Parameter smoothing | Per-sample or per-block linear/exponential ramps for every continuous control | 20–50 ms default ramp, eliminates zipper noise |
| GUI toolkit | Shared look-and-feel, knobs, meters, spectrum display, resizable vector UI, A/B slots | JUCE Graphics or a thin OpenGL layer for spectrum |
| Test harness | Null tests, impulse/frequency response capture, THD measurement, pluginval, CPU benchmark | Python (numpy/scipy) scripts driving offline renders |

## Mixing plugins

Nine plugins cover every channel and bus task in a mix. Each row names the Waves product it competes with so the target behaviour is concrete.

| Plugin | Role | Core parameters | DSP approach | Waves reference |
| --- | --- | --- | --- | --- |
| Gate / Expander | Clean up bleed, tighten drums | Threshold, range, attack, hold, release, ratio (expander mode), sidechain HP/LP, lookahead 0–5 ms, external sidechain | Dynamics core, hysteresis between open/close thresholds to avoid chatter | C1 Gate |
| Channel EQ | Tone shaping on every track | 7 bands: HP/LP (6–48 dB/oct), 2 shelves, 3 parametrics; per-band freq/gain/Q/type; analog or digital Q behaviour; spectrum analyser | Matched-response biquads; optional 2× oversampling for high-frequency accuracy | SSL E-Channel, Q10 |
| Channel Compressor | Level control, punch | Threshold, ratio (1:1–20:1 + ∞), attack 0.1–100 ms, release 10–2000 ms + auto, knee, peak/RMS, mix (parallel), sidechain HP, external key | Feedback and feed-forward topologies selectable; program-dependent release | CLA-76, CLA-2A, RCompressor |
| De-esser | Sibilance control on vocals | Frequency 2–12 kHz, threshold, range, width (band/split mode), listen, lookahead | Band-split with Linkwitz–Riley or a dynamic bell; detector on the band only | DeEsser, Sibilance |
| Saturation / Console | Analog glue and harmonic colour | Drive, character (tape / tube / transistor / clip), bias (even/odd balance), HF roll-off, mix, output | Nonlinear library with ADAA at 4× oversampling; tape mode adds hysteresis + head-bump EQ | NLS, J37, Kramer Tape |
| Delay | Rhythmic and slapback echo | Time (ms or tempo sync, L/R independent), feedback, ping-pong, filter in feedback path, modulation depth/rate, mix, ducking | Delay line + feedback filter; ducking uses dynamics core keyed from dry input | H-Delay |
| Reverb | Space and depth | Algorithm (room / hall / plate / chamber), pre-delay, decay 0.1–20 s, size, damping, diffusion, low/high cut, early/late balance, mix | Feedback delay network (8–16 lines, Hadamard mixing) for hall/room; Dattorro plate for plate | H-Reverb, RVerb, Abbey Road Plates |
| Stereo imager | Width and placement | Width 0–200% per band (3 bands), pan, mono below frequency, correlation meter | M/S encode → per-band gain → decode; Linkwitz–Riley band split | S1 Imager, Center |
| Utility / Trim | Gain staging, phase, mono checks | Gain ±24 dB, polarity flip L/R, L↔R swap, mono sum, DC block, phase rotation, channel mute | Trivial DSP; mostly a metering and workflow plugin | Not a Waves product; standard in every suite |

Optional additions once the core nine are stable: vocal level rider (Vocal Rider), transient shaper (Smack Attack), tape-style chorus/flanger.

## Mastering plugins

Seven plugins take a finished mix to a release-ready master at streaming loudness targets (−14 LUFS Spotify/YouTube, −16 Apple, −1 dBTP ceiling).

| Plugin | Role | Core parameters | DSP approach | Waves reference |
| --- | --- | --- | --- | --- |
| Linear-phase EQ | Surgical and broad tonal correction without phase shift | 8 bands, same types as Channel EQ, minimum/linear-phase switch per band, FIR length (latency vs low-frequency resolution), M/S per band | FIR via frequency sampling or windowed design; 2048–16384 taps; latency reported to host | Linear Phase EQ, F6 |
| Multiband compressor | Control bass, tame harsh mids, glue | 3–5 bands, adjustable crossover points, per-band threshold/ratio/attack/release/gain, band solo, global mix | Linkwitz–Riley 24 dB/oct crossovers (phase-coherent sum) + dynamics core per band | C6, Linear Phase MB |
| Bus / glue compressor | Cohesion across the whole mix | Threshold, ratio (2/4/10), attack (0.1/0.3/1/3/10/30 ms), release (0.1–1.2 s + auto), makeup, mix, sidechain HP | Feed-forward VCA model with slow, musical time constants; stereo-linked detector | SSL G-Master Buss, API 2500 |
| M/S processor | Width and centre balance at the master stage | Mid gain, side gain, mid/side EQ (2 bands each), width, mono-below frequency, M/S solo | M/S matrix with per-channel filter inserts | Center, S1 |
| Soft clipper | Peak shaving before the limiter for loudness | Ceiling, knee (hard → soft), oversampling 4×/8×, true-peak aware, mix | ADAA clipper with oversampling; delta/listen output | Not a Waves product; standard (e.g. StandardCLIP, KClip) |
| True-peak limiter | Final loudness and ceiling | Threshold/input gain, ceiling (dBTP), release 1–1000 ms + auto, lookahead 0–10 ms, character (transparent / punchy / loud), stereo link, dither out | 4× oversampled true-peak detection; multi-stage release (fast + slow); lookahead envelope with gain smoothing | L2, L3 Multimaximizer |
| Loudness meter & dither | Compliance and delivery | Target preset (Spotify, Apple, YouTube, broadcast), LUFS M/S/I, LRA, true-peak, history graph, dither (TPDF, noise-shaped) at 16/24-bit | BS.1770-4 K-weighted gating; dither at the very end of the chain | WLM Plus, L2 dither |

The typical chain order: Linear-phase EQ → Multiband comp → Bus comp → M/S → Soft clipper → Limiter → Meter/Dither.

## Technical requirements per plugin

Budgets assume a 48 kHz session, 64-sample buffer, on a mid-range laptop CPU; a 60-track mix should run under 40% CPU with every slot filled.

| Plugin | CPU budget (% per instance) | Latency | Oversampling | Precision | Sidechain |
| --- | --- | --- | --- | --- | --- |
| Gate | < 0.3 | 0–5 ms (lookahead) | None | 32-bit float | Yes |
| Channel EQ | < 0.5 | 0 (2× adds \~1 ms) | Optional 2× | 64-bit filters | No |
| Channel compressor | < 0.4 | 0 | None | 32-bit float | Yes |
| De-esser | < 0.4 | 0–3 ms | None | 32-bit float | No |
| Saturation | < 1.0 | \~1 ms | 4× fixed | 32-bit float | No |
| Delay | < 0.5 | 0 | None | 32-bit float | No |
| Reverb | < 2.0 | 0 | None | 32-bit float | No |
| Stereo imager | < 0.4 | 0 | None | 32-bit float | No |
| Utility | < 0.1 | 0 | None | 32-bit float | No |
| Linear-phase EQ | < 2.5 | 20–170 ms | None | 64-bit FIR | No |
| Multiband comp | < 1.5 | 0 (linear-phase mode: 20+ ms) | None | 64-bit crossovers | Yes |
| Bus compressor | < 0.5 | 0 | Optional 2× | 32-bit float | Yes |
| M/S processor | < 0.5 | 0 | None | 64-bit filters | No |
| Soft clipper | < 1.5 | \~1–2 ms | 4×/8× | 32-bit float | No |
| Limiter | < 2.5 | 1–10 ms + oversampling | 4× fixed | 32-bit float | No |
| Meter / dither | < 0.5 | 0 | 4× for true-peak only | 64-bit metering | No |

Shared requirements for every plugin:

- Formats: VST3, AU, AAX, CLAP; macOS 12+ (Universal binary), Windows 10+, Linux x86-64.
- Sample rates 44.1–192 kHz; buffer sizes 16–4096 samples; mono, stereo, and mono-to-stereo configurations.
- Resizable GUI (50–200%), full keyboard and screen-reader accessibility for parameters.
- Preset format: JSON in `ValueTree` XML, shared across formats; factory presets per plugin.
- Licensing: offline activation, no dongle; one license covers all formats.

## Build order

Six phases, ordered so each plugin reuses what the last one proved. Durations assume one experienced C++/DSP developer working full time; halve them with two.

1. **Foundation (6–8 weeks)** — framework template, filter library, dynamics core, parameter smoothing, metering FIFO, GUI toolkit, test harness. Exit: a Utility plugin passes pluginval level 10 in all four formats on all three OSes.
2. **Channel essentials (6 weeks)** — Channel EQ, Channel Compressor, Gate. These three exercise filters, dynamics and sidechain; once they sound right the core is trusted. Exit: null tests against reference transfer curves within 0.1 dB.
3. **Colour and space (6–8 weeks)** — Oversampling and nonlinear library, then Saturation, Delay, Reverb. Reverb is the hardest to tune by ear; budget half this phase for it.
4. **Stereo and vocal tools (3 weeks)** — De-esser, Stereo imager. Both are small compositions of existing modules.
5. **Mastering chain (8–10 weeks)** — Linear-phase FIR engine, then Linear-phase EQ, Multiband comp, Bus comp, M/S processor, Soft clipper, Limiter. The limiter alone is 3 weeks: it is the plugin users judge the suite by.
6. **Delivery (3 weeks)** — Loudness meter and dither, preset libraries, installer, licensing, documentation, beta round.

Total: roughly 32–38 weeks for one developer, with a usable mixing set available after phase 3 (about 20 weeks).

## Testing and validation

Every plugin ships only after passing three gates: measurement, host compatibility, and listening.

| Gate | Method | Pass criterion |
| --- | --- | --- |
| Null test | Render a reference signal through the plugin at unity/flat settings, subtract from input | Residual below −120 dBFS (EQ, utility) or −90 dBFS (dynamics at 1:1) |
| Frequency response | Sweep or impulse through each EQ band / crossover, compare to analytic curve | Within ±0.1 dB and ±2° phase of target across 20 Hz–20 kHz |
| Dynamics behaviour | Step and tone-burst tests; measure attack/release times and ratio | Within 10% of the labelled time; ratio within 0.1 |
| Aliasing | 19 kHz tone through saturation/clipper/limiter at full drive, inspect spectrum below 19 kHz | Alias products below −80 dBFS with oversampling on |
| True-peak accuracy | ITU BS.1770 and EBU Tech 3341/3342 test vectors through limiter and meter | Within ±0.1 dB LUFS and ±0.3 dB true-peak of reference |
| Stability | Fuzz every parameter at audio rate while processing noise; sample-rate and buffer-size changes mid-stream | No NaN, denormal, click or crash over 1 hour |
| Host compatibility | pluginval level 10; manual pass in Pro Tools, Logic, Ableton, Cubase, Reaper, Studio One, Bitwig | Loads, saves, automates and recalls state identically in every host |
| CPU | Benchmark 100 instances at 48 kHz / 64 samples | Within the budget table above |
| Listening | Blind A/B against the reference product on five genre-spanning mixes with two engineers | No engineer reliably prefers the reference in a blind test |

Tooling: Python (numpy, scipy, pyloudnorm) for offline analysis, pluginval for host validation, a CI job that runs the measurement suite on every commit and fails the build on regression.

## UI/UX design

The suite should feel like one instrument with 16 faces: same layout grammar, same gestures, same visual language, so an engineer who learns one plugin knows them all.

### Design principles

- One glance, one decision. The display shows what the plugin is doing to the audio (gain reduction, curve, spectrum) more prominently than the controls that cause it.
- Hands on the audio, not the menu. Every primary parameter is reachable in one gesture; secondary ones live in a collapsible strip, never a modal.
- Honest comparison. Gain-matched bypass, A/B/C/D slots, and a delta-listen button are on the header of every plugin, in the same place.
- Dark by default, light optional, both high contrast. Colour carries meaning (gain reduction, band identity, clipping) and nothing else.
- Resizable and vector. The UI scales 50–200% without a single raster asset.

### Shared chrome

| Zone | Contents | Behaviour |
| --- | --- | --- |
| Header bar | Plugin name, preset browser, A/B/C/D slots, undo/redo, gain-match bypass, delta listen, resize, settings gear | Identical across all 16 plugins; preset browser opens as an overlay with search, tags, favourites |
| Main display | The plugin’s “truth”: EQ curve + spectrum, compressor transfer curve + GR history, reverb decay envelope, stereo vectorscope | Always the largest element; draggable where it makes sense |
| Control strip | Primary knobs/sliders in use order, left to right | Double-click resets; shift-drag for fine; alt-click to type a value; right-click for automation and MIDI learn |
| Advanced drawer | Sidechain, oversampling, detector mode, routing | Collapsed by default; remembered per preset |
| Footer | Input/output meters, latency readout, CPU hint, output trim | Shared metering component; trim is the last thing in the chain |

### Per-family interaction ideas

| Family | Signature interaction |
| --- | --- |
| EQs | Drag nodes directly on the curve; scroll for Q; hold a node to solo its band; live spectrum behind the curve with peak-hold; linear-phase EQ shows latency cost as you lengthen the FIR |
| Compressors / limiter | Transfer curve with a live input dot riding it; gain-reduction history scrolling under the curve; threshold dragged on the waveform history itself; limiter shows true-peak overs as red ticks on the timeline |
| Multiband | Spectrum with draggable crossover lines; each band’s GR drawn as a shaded region over its frequency range; click a band to focus its controls |
| Gate / de-esser | Waveform history with open/close shading (gate) or detected sibilant regions highlighted (de-esser); listen-to-what-is-removed button |
| Saturation | A single large drive knob plus a harmonic spectrum showing even/odd content as you turn it; character selector as tabs with a short description each |
| Delay / reverb | Visual tap or decay display you can drag (delay time, decay length); tempo-sync grid snaps when dragging near a note value |
| Imager / M/S | Vectorscope or goniometer with a correlation bar; width drawn as the spread of the Lissajous figure; mono-below shown as a frequency line on a small spectrum |
| Meter / dither | Target preset buttons at top; big integrated LUFS number; short-term history ribbon; red/green compliance badge; dither settings tucked away |

### Visual language

- Typography: one geometric sans (Inter or similar) at two weights; numbers in tabular figures so readouts don’t jitter.
- Palette: near-black background, one accent per plugin family (EQ = blue, dynamics = amber, time/space = teal, stereo = violet, mastering = white/gold) so a glance at the mixer window tells you what is loaded.
- Knobs: flat arc with a bright value sweep, no skeuomorphic chrome; the value label sits under the knob and becomes editable on click.
- Motion: 100–150 ms eased transitions for state changes; meters at 60 fps; no decorative animation.

### Workflow features

- Preset browser shared across the suite with tags (genre, instrument, task) and a “similar presets” suggestion.
- Chain templates: save and recall a whole mastering chain across plugins from any one of them.
- Auto gain-match on bypass; loudness-matched A/B so a louder setting never wins by default.
- MIDI learn and macro knobs for hardware controllers.
- Full keyboard navigation: tab through controls, arrow keys adjust, screen-reader labels on every parameter.

## Gaps and the AI layer

### What the core suite is still missing

Compared with a full Waves-class catalogue, these are the plugins users will ask for first. None blocks a release, but the first four belong in a phase 7.

| Plugin | Why it matters | Approach | Waves reference |
| --- | --- | --- | --- |
| Pitch correction | Expected on every vocal | Pitch tracking (pYIN or CREPE-tiny) + PSOLA/phase-vocoder retune, scale/key input, speed and humanise | Tune, Tune Real-Time |
| Vocal level rider | Replaces hours of clip-gain automation | Slow RMS follower with target level, range and sensitivity; writes automation | Vocal Rider |
| Noise and restoration | Podcasts, live recordings, bleed | Neural denoise/de-reverb (see models below); classic spectral gate as fallback | Clarity Vx, X-Noise, NS1 |
| Reference-match EQ | “Make it sound like this track” | Long-term spectrum of reference vs source → matching EQ curve with smoothing and amount | Curves Equator |
| Transient shaper | Punch without compression | Fast/slow envelope difference, attack and sustain gain | Smack Attack |
| Convolution reverb | Real spaces and hardware | Partitioned FFT convolution, IR library, true-stereo | IR-1 |
| Stem separation | Remixing, de-bleeding | Neural (HTDemucs) offline or near-real-time | Not a Waves product; standard elsewhere |
| Immersive / Atmos panner | Streaming spatial delivery | Object panning, bed routing, binaural monitor | Nx, Atmos tooling |

### Two distinct AI layers

Keep these separate in the architecture; they have different latency budgets, models and failure modes.

| Layer | What it does | Runs where | Models | Latency |
| --- | --- | --- | --- | --- |
| Neural DSP | Processes audio directly: denoise, de-reverb, de-bleed, stem split, analog capture | Inside the plugin, audio thread, via statically linked ONNX Runtime (CPU, CoreML, DirectML) | Small task-specific nets (1–20 MB), shipped in the installer | Real-time, 10–50 ms lookahead |
| Prompt layer | Turns a sentence plus audio analysis into parameter settings | Inside the plugin bundle, worker thread, via embedded llama.cpp; no network, no separate runtime | One small instruction-tuned LLM (0.5–1.5 B, 4-bit) shared by every plugin, shipped in the installer | 0.3–2 s on CPU, applied as smoothed automation |

### How the prompt layer works

Example: “make this kick more punchy with a fast attack.”

1. The plugin extracts features from the last few seconds: LUFS, crest factor, transient density, spectral centroid and tilt, low/mid/high energy, stereo width, detected instrument class.
2. The prompt, the features, the plugin’s current parameters and its tool schema go to the model.
3. The model returns a JSON parameter set with a one-line rationale (for the kick: attack 0.5–2 ms, ratio 4:1, threshold set so GR is 3–6 dB, release \~80 ms, parallel mix 60%).
4. The plugin validates ranges, previews with gain-match, and shows a diff: current → proposed. Apply, tweak or undo.
5. Every accepted or rejected suggestion is logged (locally, opt-in) as training data for the next model version.

Research this builds on: Sony’s LLM2Fx (zero-shot text → EQ and reverb parameters) and LLM2Fx-Tools (fine-tuned LLM emitting tool-call sequences for effect chains), Northwestern’s Text2FX (text to effects via differentiable DSP), and the MixAssist dataset of professional mixing conversations.

### Open-weight models worth leveraging

| Model | Use in the suite | Licence (check current) | Shipped size |
| --- | --- | --- | --- |
| Qwen2.5 0.5 B or 1.5 B, fine-tuned on prompt → parameter pairs, Q4 quantised | The prompt-to-parameter model; one copy shared by all plugins | Apache 2.0 | 0.4–1.0 GB |
| DeepFilterNet | Real-time speech denoise | MIT / Apache 2.0 | \~2 MB |
| HTDemucs (Meta), ONNX export | Stem separation, de-bleed (offline render, not real-time) | MIT | \~80 MB |
| CLAP (LAION), audio tower only, quantised | Semantic preset search, “sounds like” matching | Apache 2.0 | \~150 MB |
| whisper.cpp tiny or base | Voice prompts | MIT | 40–150 MB |
| Neural Amp Modeler cores | Analog hardware captures for the saturation plugin | MIT | per-capture, under 1 MB |
| MERT (research only) | Training-time music features, not shipped | CC-BY-NC | — |

Left out on purpose: Qwen2-Audio and other 7 B+ audio-language models (too large to bundle, too slow on CPU), Stable Audio Open (generation, not processing), and any cloud API. Everything the user runs ships in the installer and works with the network unplugged.

### Offline packaging rules

- One installer, one bundle. The LLM, neural DSP models, llama.cpp and ONNX Runtime live inside the plugin bundle (macOS) or the plugin’s resource folder (Windows, Linux). No Python, no Docker, no separate app, no account.
- Shared model host. The first plugin instance to need the LLM loads it into a process-wide singleton; the other 60 instances share it. Memory cost is paid once per DAW session, not per plugin.
- Hardware acceleration without drivers: Metal on macOS, DirectML on Windows, CPU fallback everywhere. Nothing the user has to install or configure.
- Installer size target: 1.5–2.5 GB with all models, offered as a Full installer and a Lite installer (DSP only, no AI, under 100 MB) for users who do not want the AI layer.
- Model updates arrive only through plugin updates, with a visible version in the settings panel.
- The fine-tuned model is the moat: a 0.5–1.5 B model trained on 2–5 k prompt → parameter pairs outperforms a general 8 B model on this narrow task and fits the size budget.

### What it takes to ship prompting

- Feature extraction module in the shared core (2 weeks).
- A JSON tool schema per plugin, generated from the parameter layout so it never drifts (1 week).
- Embedded llama.cpp runtime with a shared model host, prompt templates and strict JSON validation (3 weeks).
- Training data and fine-tune: 2–5 k prompt → parameter pairs built from the MixAssist, SocialFX and Text2FX datasets plus engineer-written examples; QLoRA fine-tune on Qwen2.5 0.5 B / 1.5 B; quantise to Q4 (4 weeks, ongoing).
- Evaluation set: 300–500 held-out prompts scored by an engineer; the model ships only when it beats the general model and a rules-based baseline (2 weeks).
- UI: prompt bar in the header, proposal diff view, preview/apply/undo, history (2 weeks).
- Neural DSP track in parallel: ONNX Runtime integration and the first denoise plugin (4 weeks).
- Installer work: bundling models, Full vs Lite builds, code signing with large payloads (1 week).

Total: 8–12 weeks on top of the core suite, with the prompt layer usable on the compressor and EQ after about 6.

### UI for the AI layer

- A single prompt bar in the shared header, invoked with a keystroke, accepting typed or spoken input.
- The proposal appears as highlighted deltas on the existing controls (ghost knob positions), never as a modal; one tap applies, one tap reverts.
- A short rationale line under the bar (“fast attack lets the click through, 4:1 adds weight”) so users learn as they go.
- A “why” affordance on every suggestion, and a permanent “Offline · on this machine” badge so users know their audio never leaves the computer.

## Fine-tuning specification

The prompt-to-parameter model is a first-class deliverable with its own pipeline, tests and release gate. It is built as phase 7, in parallel with phase 5 of the DSP build, and ships only when it passes the gate below.

### Target

| Item | Spec |
| --- | --- |
| Base model | Qwen2.5-0.5B-Instruct for the first release; Qwen2.5-1.5B-Instruct if 0.5 B misses the accuracy gate. Apache 2.0 |
| Method | QLoRA (rank 16–32, all attention and MLP projections), 2–4 epochs, then merge and quantise to GGUF Q4\_K\_M |
| Shipped size | 0.4 GB (0.5 B) or 1.0 GB (1.5 B) |
| Inference | Embedded llama.cpp, CPU by default, Metal/DirectML when present; under 2 s to first full JSON on a 2020 laptop CPU |
| Context | 2 k tokens: system prompt + tool schema + audio features + current parameters + user prompt |
| Output | Strict JSON only: parameter set, confidence 0–1, one-sentence rationale; grammar-constrained decoding so invalid JSON is impossible |

### Input and output contract

The model never sees audio. It sees a compact feature block the plugin computes, so the training data is fully synthetic-friendly and the model stays small.

- Input features (per plugin, from the last 4–8 s): integrated and short-term LUFS, crest factor, peak dBTP, spectral centroid and tilt, energy in 6 bands, transient density per second, stereo width and correlation, detected source class (kick, snare, bass, vocal, guitar, keys, full mix, other) with confidence.
- Input state: the plugin’s current parameters in named, unit-bearing form (`attack_ms: 25`, not a normalised float).
- Input prompt: free text, plus optional conversation history of the last 3 turns inside the same plugin.
- Output: `{"params": {"attack_ms": 1.0, "ratio": 4.0, "threshold_db": -18.5, "release_ms": 80, "mix_pct": 60}, "confidence": 0.82, "rationale": "Fast attack keeps the click; 4:1 adds weight without pumping."}`
- One schema per plugin, generated from the plugin’s parameter layout at build time, so a renamed knob fails the build rather than the model.

### Dataset

Target 5,000 examples for release one, growing with each release. Authoring is split: Claude drafts the prompt → parameter pairs from established engineering practice, engineers verify them against real stems inside the plugins, and only verified pairs count as gold. Sources, in priority order:

| Source | Share | How |
| --- | --- | --- |
| AI-drafted, engineer-verified (gold) | 30% | Claude writes 1,500 pairs covering every plugin, source class and prompt style (absolute settings, relative edits, multi-turn refinements); two mixing engineers review each in the plugin with the matching stem and correct or reject. Only pairs that pass review enter the gold set |
| Programmatic augmentation | 40% | Each gold pair rephrased 5–10 ways (slang, typos, non-native English, French and Kinyarwanda prompts), with feature blocks regenerated from the same stem |
| Derived from public datasets | 20% | MixAssist conversations, SocialFX and Text2FX descriptors, mapped onto the suite’s parameter space by rule and then checked by an engineer |
| Counter-examples | 10% | Claude-drafted vague, contradictory or out-of-scope prompts (“make it better”, “add reverb” inside an EQ) with the correct behaviour: ask a clarifying question or decline, never guess; engineer-verified like the gold set |

Rules: every example is tied to a feature block computed from a real stem by the suite’s own extractor, never a hand-typed or synthetic one; engineers supply at least 50 stems per source class for this purpose; every parameter is within the plugin’s range; the gold set is held out of augmentation of its own test split; opt-in telemetry from released plugins (accepted and rejected proposals, local only until the user exports) feeds the next round.

AI-drafting workflow: Claude produces examples in batches of 100 per plugin as JSONL matching the output contract above, each with the prompt, intended source class, target parameters and a one-line rationale. The first batch is reviewed in full before the rest is generated so the drafting style is corrected early. Expected engineer effort drops from authoring (about 3 weeks for two engineers) to verification (about 3–4 days), because reviewing a proposed setting with a stem loaded takes under a minute. The known limitation is that AI-drafted examples encode consensus practice rather than taste; the listening-test gate and the telemetry loop are what add character over subsequent releases.

### Training recipe

- Split 80/10/10 by stem, not by prompt, so paraphrases of a test example never leak into training.
- Loss on the JSON output only; the system prompt and features are masked.
- Curriculum: single-parameter edits first (“faster attack”), then full settings from scratch, then multi-turn refinements (“a bit less”).
- Train on one consumer GPU (24 GB) in under 6 hours for 0.5 B; reproducible from a single script with pinned seeds and dependency versions.
- Export: merge LoRA → safetensors → GGUF → Q4\_K\_M; keep the F16 GGUF for regression comparison.

### Evaluation and release gate

| Metric | How measured | Gate |
| --- | --- | --- |
| Schema validity | Share of outputs that parse and are in range | 100% (grammar-constrained) |
| Parameter accuracy | Mean absolute error per parameter against the engineer setting, in perceptual units (dB, ms on a log scale, ratio on a log scale) | Within 1.5 dB / 25% time / 20% ratio on 85% of the held-out set |
| Direction accuracy | For relative prompts (“brighter”, “tighter”), does every changed parameter move the right way | 95% |
| Listening test | 100 held-out prompts, engineer blind-picks between the model’s setting and the gold setting | Model wins or ties 60%+ |
| Refusal behaviour | Counter-example set | Clarifies or declines 90%+, never emits a guess |
| Baselines | Same tests against untuned Qwen2.5-7B zero-shot and a rules-based mapper | Fine-tuned small model must beat both |
| Speed | Time to full JSON, p95, on reference CPU | Under 2 s |
| Regression | Full suite re-run on every model candidate; results stored with the model hash | No metric drops more than 2 points from the shipped model |

### Artefacts and versioning

- The repository ships `data/` (gold set, augmentation scripts, derived-data mappers), `train/` (one script per model size), `eval/` (the gate above as an automated suite) and `export/` (GGUF build).
- Each shipped model carries a version string, its training-data snapshot hash and its eval report, visible in the plugin’s settings panel.
- Models are swapped only via plugin updates; a plugin refuses to load a model file whose schema version does not match its own.

### Timeline (phase 7, parallel with DSP phase 5)

1. Weeks 1–2: feature extractor and schema generator in the shared core; data tooling.
2. Weeks 3–6: gold set authoring (engineers), augmentation and public-data mapping; first 0.5 B fine-tune at week 4 to shake out the pipeline.
3. Weeks 7–8: full training runs on 0.5 B and 1.5 B, eval suite, listening test, pick the shipping model.
4. Weeks 9–10: llama.cpp embedding, shared model host, prompt bar UI, installer integration, Lite and Full builds.
5. Ongoing: monthly retrain as telemetry and new gold examples arrive.

### Compute and budget

| Resource | Role | Cost |
| --- | --- | --- |
| Developer Mac (Apple silicon, 8 GB) | Data tooling, feature extraction, eval harness, 0.5 B LoRA iteration runs with MLX (2–3 GB peak, 1–3 hours per run), representative inference testing | Owned |
| Rented RTX 4090 24 GB (RunPod or similar) | 1.5 B QLoRA candidates (about 3 hours each), 7 B zero-shot baseline, F16 eval and GGUF export; secure tier for final candidates, community/spot tier for exploration | About $0.35–0.70 per hour |
| Storage | 100 GB persistent volume for datasets, checkpoints and eval reports | About $7 per month |

Per release cycle: roughly 40 GPU-hours including failed runs, $35–50. First year with monthly retrains: $300–600. A one-off hyperparameter sweep adds $20–40. GPU cost is negligible next to engineer verification time, which is the budget line to protect.
