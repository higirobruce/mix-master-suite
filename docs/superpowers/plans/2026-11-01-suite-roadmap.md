# Mix & Master Plugin Suite — Implementation Roadmap (M0–M7)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. This roadmap is executed one milestone at a time; M0 and M1 have their own step-level plans (`2026-11-01-m0-scaffold.md`, `2026-11-01-m1-core-dsp.md`). Each later milestone gets its step-level plan written at the start of that milestone, from the task list here, into `docs/STATE.md` and `docs/superpowers/plans/`.

**Goal:** Ship 16 release-quality mixing and mastering plugins (VST3/AU/AAX/CLAP/Standalone) on one shared C++20/JUCE 8 core, plus an offline prompt-to-parameter AI layer, with every spec exit criterion proven by an automated measurement suite in CI.

**Architecture:** One static library `mm_core` holds all DSP (filters, dynamics, smoothing, metering, delay, oversampling, nonlinear). One `mm_plugin_base` library holds the JUCE processor base (APVTS from generated parameter tables, state, presets, gain-matched bypass, trim, latency, metering FIFO) and one `mm_gui` library holds the shared chrome. Each plugin is a folder with `params.json`, a Processor that composes core modules, an Editor that composes gui components, and presets. A Python 3.11 harness drives an offline render tool (`mm_render`) to measure every exit criterion; CI runs it on macOS, Windows and Linux.

**Tech Stack:** C++20, JUCE 8.0.x (FetchContent, SHA-pinned), clap-juce-extensions (SHA-pinned), Catch2 v3 (FetchContent, SHA-pinned), CMake ≥ 3.25 + Ninja, Python 3.11 (numpy, scipy, soundfile, pyloudnorm, pytest; pinned in `tests/requirements.txt`), pluginval (pinned release binary, sha256-checked), GitHub Actions (macos-14, windows-2022, ubuntu-22.04), llama.cpp + ONNX Runtime (M7), MLX / PyTorch+PEFT (M7).

**Spec:** `docs/SPEC.md` (source of truth) and `CLAUDE.md` (process, hard rules, milestone list). Mockups: `docs/Mix & Master Plugin UI Mockups.html` (six boards: Channel EQ, True-Peak Limiter, Loudness Meter, Analogue Channel EQ, Analogue Bus Compressor, AI prompt bar on Channel Compressor).

## Global Constraints

Copied from the spec and CLAUDE.md. Every task's requirements implicitly include these.

- Formats: VST3, AU, AAX, CLAP (+ Standalone for dev); macOS 12+ Universal binary, Windows 10+, Linux x86-64. AAX is stubbed (format skipped) when the SDK is absent.
- Sample rates 44.1–192 kHz; buffer sizes 16–4096 samples; mono, stereo, and mono-to-stereo configurations. Every DSP module tested at 44.1k, 48k, 96k, 192k.
- Audio thread: no allocation, no locks, no I/O, no logging, no exceptions. All buffers sized in `prepareToPlay`. Debug builds run `-fsanitize=address,undefined`; a sanitizer hit fails the milestone.
- Every continuous parameter is smoothed (20–50 ms default ramp, 20 ms chosen); every parameter has unit, range, default, skew in `params.json`, the single source for layout, automation, GUI and the AI tool schema (generated at build time).
- Nonlinear stages always run oversampled. Latency (oversampling + lookahead + FIR) always reported to the host.
- Gain-matched bypass and an output trim on every plugin; delta-listen, A/B/C/D, undo in the shared header.
- Null test: residual below −120 dBFS (EQ, utility) or −90 dBFS (dynamics at 1:1). Frequency response within ±0.1 dB and ±2° of the analytic curve, 20 Hz–20 kHz. Dynamics attack/release within 10%, ratio within 0.1. Aliasing products below −80 dBFS at 19 kHz with oversampling on. Loudness within ±0.1 LU and true-peak within ±0.3 dB of EBU Tech 3341/3342 vectors. Stability: 1 hour parameter fuzz at audio rate, SR/buffer changes mid-stream, no NaN/denormal/click/crash. pluginval strictness level 10 on all three OSes.
- CPU budget per instance (48 kHz, 64 samples, 100 instances): Gate < 0.3%, Channel EQ < 0.5%, Channel comp < 0.4%, De-esser < 0.4%, Saturation < 1.0%, Delay < 0.5%, Reverb < 2.0%, Imager < 0.4%, Utility < 0.1%, LP-EQ < 2.5%, Multiband < 1.5%, Bus comp < 0.5%, M/S < 0.5%, Soft clipper < 1.5%, Limiter < 2.5%, Meter/dither < 0.5%.
- Precision: 64-bit filters (Channel EQ, LP-EQ FIR, multiband crossovers, M/S filters, metering); 32-bit float elsewhere.
- Dependencies pinned to exact versions by SHA via FetchContent; nothing fetched at plugin runtime; no network calls at plugin runtime, ever.
- Conventional commits on `milestone/<name>` branches; never commit a broken build to `main`; ADR in `docs/adr/NNNN-title.md` before architecture code lands.
- GUI: vector only, resizable 50–200%, dark default + light theme, one accent per family (EQ #5B9DE8 blue, dynamics #E0B45C amber, time/space teal, stereo violet, mastering #D8DDE4 white/gold), Inter-like geometric sans with tabular figures, 100–150 ms eased transitions, meters at 60 fps.
- Preset format: `ValueTree` XML with a JSON `meta` string (tags, author, notes); factory presets per plugin in `plugins/<name>/presets/`.
- AI: everything offline, model inside the bundle, shared process-wide model host, grammar-constrained JSON, 2 k context, under 2 s p95 to full JSON on a 2020 laptop CPU; the plugin refuses a model whose schema version does not match.

## Review Focus

Inputs the spec implies but no exit criterion names directly. Each has a test pinned in the owning milestone's plan.

1. **Host changes block size and sample rate between calls without a new `prepareToPlay`** (some hosts shrink the last buffer; Reaper varies block size). Expected: every plugin handles any block ≤ the prepared maximum, including 0 samples, and a true SR change arrives via `prepareToPlay` without clicks. Test: `test_variable_block_sizes` in M0 (Utility) and each later plugin.
2. **Silence, DC, and denormal-range input after loud input** (fade-outs). Expected: no denormal CPU spike, no NaN/Inf ever reaches the output, DC blocked where the plugin claims. Test: `test_denormal_tail` (ScopedNoDenormals + NaN counter in `mm_render` report) in M0 base, inherited by all plugins.
3. **Parameter at min, max, default, and snapped within one block** (automation jumps). Expected: no clicks above the smoothing ramp, no divide-by-zero at ratio 1:1 / Q min / time 0. Test: `test_param_extremes` per plugin, driven by `params.json` so new parameters are covered automatically.
4. **State round-trip across formats and versions** (preset saved in VST3 loaded in AU; old preset missing a new parameter). Expected: identical parameter values, unknown params ignored, missing params at default, A/B/C/D slots restored. Test: `test_state_roundtrip` + `test_state_forward_compat` in M0 base.
5. **Mono-to-stereo and mono instances of stereo-designed plugins** (imager, M/S, ping-pong delay, stereo-linked detectors). Expected: mono input processed without reading an absent channel; stereo-only features degrade gracefully (width = no-op, link = self). Test: `test_channel_configs` in M0 base, with per-plugin overrides where behaviour differs.

---

## File Structure (whole repo)

```
CMakeLists.txt                 root: options, FetchContent (JUCE, clap-juce-extensions, Catch2), subdirs
cmake/Dependencies.cmake       pinned SHAs and hashes, one place
cmake/MMPlugin.cmake           mm_add_plugin(<name>) : params codegen, juce_add_plugin, formats, binary data
cmake/Sanitizers.cmake         MM_SANITIZE option
cmake/Warnings.cmake           -Wall -Wextra -Werror set
core/                          static lib mm_core, namespace mm, header-mostly templates
  CMakeLists.txt
  common/   Math.h (dbToGain, gainToDb, fastTanh), Denormals.h, Assert.h
  smoothing/Smoother.h
  delay/    IntegerDelay.h, FractionalDelayLine.h, TempoSync.h
  filters/  Biquad.h, MatchedBiquad.h, SVF.h, FIRFilter.h, FIRDesign.h, LinkwitzRiley.h, OnePole.h, AllPass.h
  dynamics/ Detector.h, EnvelopeFollower.h, GainComputer.h, Lookahead.h, SidechainFilter.h
  oversampling/Oversampler.h
  nonlinear/Waveshapers.h, ADAA.h, TapeHysteresis.h, TubeTransfer.h
  metering/ MeterFifo.h, PeakMeter.h, RmsMeter.h, Correlation.h, Spectrum.h, LoudnessMeter.h (M5), TruePeak.h (M5)
  fir/      PartitionedConvolver.h, LinearPhaseDesign.h (M5)
plugin_base/                   static lib mm_plugin_base (JUCE): PluginProcessorBase, ParamTable, PresetManager,
                               GainMatchedBypass, StateIO, AbcdSlots, LatencyReporter
gui/                           static lib mm_gui: Theme.h, LookAndFeel, Knob, ToggleButton, Selector,
                               HeaderBar, Footer, MeterBar, SpectrumDisplay, CurveDisplay, HistoryDisplay,
                               PluginEditorBase, PresetBrowserOverlay, AdvancedDrawer, PromptBar (M7)
plugins/<name>/                params.json, <Name>Processor.{h,cpp}, <Name>Editor.{h,cpp}, presets/*.mmpreset, CMakeLists.txt
tools/
  gen_params.py                params.json → Params.h + tool_schema.json (+ GBNF in M7)
  render/                      mm_render JUCE console app: plugin/module offline render, bench, dump-params
  run_pluginval.sh             downloads pinned pluginval (sha256), runs level 10
  bench_cpu.py                 100-instance benchmark via mm_render --bench
  measure.py                   per-plugin measurement entry point (wraps pytest selections)
  installer/                   M6: pkgbuild/Inno/AppImage scripts, Full vs Lite
tests/
  requirements.txt             pinned Python deps
  harness/                     render.py, signals.py, measure.py, loudness.py, fixtures.py
  core/                        pytest per core module (uses mm_render --module)
  plugins/                     pytest per plugin
  cpp/                         Catch2 unit tests, one file per core module
  fixtures/                    EBU vectors (M5), reference sweeps, stems placeholder
ai/                            M7: features/, schema/, llm/, prompts/
ml/                            M7: data/, train/, eval/, export/
docs/                          SPEC.md, STATE.md, REVIEW.md, adr/, superpowers/plans/
.github/workflows/ci.yml       matrix build + ctest + pytest + pluginval + sanitizer job
```

## Core Interfaces (fixed names; every plugin and plan references these)

All in namespace `mm`. Templates take `T` = `float` or `double`. Every stateful class has `void prepare(double sampleRate, int maxBlockSize)` and `void reset()`; nothing allocates after `prepare`.

```cpp
// smoothing/Smoother.h
template<typename T> class Smoother {            // M0 linear, M1 adds exponential
  void prepare(double sampleRate, double rampMs);
  void setTarget(T v); void snap(T v);           // snap = no ramp
  T next(); void skip(int n); bool isSmoothing() const; T current() const; };

// delay/IntegerDelay.h                               M0
template<typename T> class IntegerDelay { void prepare(int maxSamples, int channels); void setDelay(int); T process(T in, int ch); };

// delay/FractionalDelayLine.h                        M1
enum class DelayInterp { Lagrange3, Thiran1 };   // nested aliases: FractionalDelayLine<T>::Interp
template<typename T> class FractionalDelayLine { using Interp = DelayInterp; void prepare(double sr, double maxSeconds, int channels); void setInterp(Interp); void setSlewMs(double);
  void setDelaySamples(float d, int ch); void push(T in, int ch); T read(int ch); T readAt(float delaySamples, int ch); };
double TempoSync::noteSeconds(double bpm, int division /*1,2,4,8,16,32*/, bool dotted, bool triplet);

// filters/Biquad.h                                   M1  (TDF-II, double coefficients)
template<typename T> struct BiquadCoeffs { T b0,b1,b2,a1,a2; };
template<typename T> class Biquad { void setCoeffs(const BiquadCoeffs<T>&); T process(T x); void process(T* buf, int n); void reset(); };
namespace rbj { BiquadCoeffs<double> peak(double sr, double f, double gainDb, double q); lowShelf(...); highShelf(...); lowPass(sr,f,q); highPass(sr,f,q); notch(sr,f,q); allPass(sr,f,q); }
namespace matched { BiquadCoeffs<double> peak(sr,f,gainDb,q); lowShelf(...); highShelf(...); lowPass(sr,f,q); highPass(sr,f,q); }   // Vicanek 2016
std::complex<double> evaluate(const BiquadCoeffs<double>&, double sr, double freqHz);   // analytic H(e^jw) for tests

// filters/SVF.h                                      M1  (Zavalishin TPT)
enum class SvfMode { LowPass, HighPass, BandPass, Notch, Peak, LowShelf, HighShelf, AllPass };
template<typename T> class SVF { using Mode = SvfMode; void prepare(double sr); void setMode(Mode); void setParams(double freq, double q, double gainDb); T process(T x); std::complex<double> evaluate(double hz) const; };

// filters/OnePole.h                                  M0 (DC block), M1 (tests)
template<typename T> class OnePoleHP { void setCutoff(double hz); T process(T x); };

// filters/AllPass.h                                  M0 (phase rotation), M1 (tests)
template<typename T> class FirstOrderAllPass { void setCorner(double hz); T process(T x); };

// filters/FIRFilter.h + FIRDesign.h                  M1
template<typename T> class FIRFilter { void prepare(int maxTaps, int channels); template<class U> void setTaps(const U* taps, int n /*clamped to maxTaps*/); T process(T x, int ch); int latencySamples() const; };
std::vector<double> fir::windowedSinc(int taps, double cutoffNorm, Window w);  std::vector<double> fir::halfBand(int taps, double transitionNorm);

// filters/LinkwitzRiley.h                            M1
enum class LrOrder { LR2 = 2, LR4 = 4, LR8 = 8 };
template<typename T> class LinkwitzRiley { using Order = LrOrder; void prepare(double sr); void setCrossover(double hz); void setOrder(Order);
  void process(T x, T& low, T& high); std::complex<double> evaluateLow(double hz) const; evaluateHigh(double hz) const; };   // plugins use T = double

// dynamics/                                          M1
enum class DetectorMode { Peak, Rms };
template<typename T> class Detector { using Mode = DetectorMode; void prepare(double sr); void setMode(Mode); void setRmsWindowMs(double); T process(T x); };   // returns linear level
template<typename T> class EnvelopeFollower { void prepare(double sr); void setAttackMs(double); void setReleaseMs(double); void setAutoRelease(bool); T process(T valueDb); void snapTo(T); };  // log-domain 10-90 % times; applied to the gain reduction (Giannoulis), see ADR-0004
struct GainComputerParams { double thresholdDb, ratio, kneeDb; bool expander; double rangeDb; };
template<typename T> class GainComputer { void set(const GainComputerParams&); T gainDb(T levelDb) const; };
template<typename T> class Lookahead { void prepare(double sr, double maxMs, int channels); void setMs(double); T delaySignal(T x, int ch); T maxEnvelope(T levelIn); int latencySamples() const; };
template<typename T> class SidechainFilter { void setHighPass(double hz); void setLowPass(double hz /*0=off*/); T process(T x); };

// oversampling/Oversampler.h                         M1  (wraps juce::dsp::Oversampling, FIR equiripple half-band)
class Oversampler { void prepare(double sr, int maxBlock, int channels, int factor /*1,2,4,8*/); juce::dsp::AudioBlock<float> up(juce::dsp::AudioBlock<float>); void down(juce::dsp::AudioBlock<float>); int latencySamples() const; double oversampledRate() const; int factor() const; };   // header-only; mm_core is INTERFACE and links juce::juce_dsp

// nonlinear/                                         M1
namespace shape { float tanhS(float x); float atanS(float x); float asym(float x, float bias); float softClip(float x, float knee); float hardClip(float x); }
template<float (*Fn)(float), double (*Ad1)(double)> class ADAA1 { float process(float x); void reset(); };   // first-order ADAA, antiderivative difference in double; midpoint fallback when |x-x1|<1e-6
class TapeHysteresis { void prepare(double sr); void set(double drive, double saturation, double width); float process(float x); };   // Jiles-Atherton, RK4
class TubeTransfer  { void set(double drive, double bias); float process(float x); };

// metering/                                          M0 FIFO + Peak/RMS, M1 Correlation/Spectrum, M5 Loudness/TruePeak
template<typename T, int Capacity> class MeterFifo { bool push(const T&); bool pop(T&); int available() const; };   // SPSC ring on std::atomic (mm_core has no JUCE dependency until M1 T12)
struct MeterFrame { float peakIn[2], peakOut[2], rmsIn[2], rmsOut[2], gainReductionDb, correlation, latencyMs; };
class LoudnessMeter { void prepare(double sr, int channels); void process(const float* const* in, int n); double momentary() const; double shortTerm() const; double integrated() const; double range() const; void resetIntegrated(); };
class TruePeakMeter { void prepare(double sr, int channels); void process(...); double maxDbTP() const; int overs(double ceilingDbTP) const; };
```

Plugin base (namespace `mm`, library `mm_plugin_base`):

```cpp
class PluginProcessorBase : public juce::AudioProcessor {
protected:
  explicit PluginProcessorBase(const ParamTable& table, BusesProperties);
  // subclass hooks (called with host blocks; base handles bypass, delta, trim, metering, latency)
  virtual void prepareDsp(double sr, int maxBlock, int channels) = 0;
  virtual void processDsp(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) = 0;
  virtual void resetDsp() = 0;
  virtual int dspLatencySamples() const { return 0; }        // base adds nothing; subclass returns OS+lookahead+FIR
  virtual juce::ValueTree extraState() { return {}; } virtual void restoreExtraState(const juce::ValueTree&) {}
  float param(ParamId id) const;                              // raw (unsmoothed) current value
  Smoother<float>& smoothed(ParamId id);                      // per-continuous-param smoother, 20 ms default from params.json
public:
  juce::AudioProcessorValueTreeState& apvts();
  PresetManager& presets(); AbcdSlots& slots(); juce::UndoManager& undo();
  MeterFifo<MeterFrame, 64>& meterFifo();
  void setDeltaListen(bool); bool deltaListen() const;        // UI-only, atomic, not a parameter
  // AudioProcessor overrides are final in the base: prepareToPlay, releaseResources, processBlock, processBlockBypassed,
  // getStateInformation, setStateInformation, isBusesLayoutSupported (mono, stereo, mono→stereo), getName, hasEditor, etc.
};
```

Reserved parameter ids added to every plugin by the generator: `bypass` (bool, default false), `trim_db` (float, −12..+12 dB, default 0, skew 1, smoothing 20 ms, `ai: false`).

## Milestone Plans

Exit criteria are copied verbatim from the spec / CLAUDE.md. Tasks are sized under ~2 hours. Each milestone ends with the Report task: `docs/REVIEW.md`, tag `review/<milestone>`, line `READY FOR REVIEW: <milestone>`.

### M0 Scaffold — step-level plan in `2026-11-01-m0-scaffold.md`

Exit criteria (CLAUDE.md): "repo layout, CMake, CI matrix green, Utility plugin builds in all formats, pluginval level 10 passes on macOS/Windows/Linux. ADR-0001 build system, ADR-0002 parameter model." Spec phase 1 exit: "a Utility plugin passes pluginval level 10 in all four formats on all three OSes." (AAX stubbed: documented in ADR-0001 and REVIEW.md as a known gap.)

Tasks: T0 toolchain + git; T1 ADR-0001/0002; T2 root CMake + dependency pins + warnings/sanitizers; T3 `gen_params.py` + schema; T4 `mm_core` minimal (Math, Smoother linear, IntegerDelay, OnePoleHP, FirstOrderAllPass, MeterFifo, Peak/RMS) + Catch2; T5 `mm_plugin_base`; T6 Utility processor + params.json; T7 `mm_render` tool; T8 Python harness + Utility measurement tests; T9 `mm_gui` chrome + Utility editor; T10 pluginval runner + bench_cpu + CI; T11 presets + STATE/REVIEW + tag.

### M1 Core DSP — step-level plan in `2026-11-01-m1-core-dsp.md`

Exit criteria (CLAUDE.md): "filters (biquad, matched, FIR, Linkwitz–Riley), dynamics core, smoothing, metering FIFO, delay line, oversampling, nonlinear/ADAA. Each with unit tests, frequency/step-response measurement tests, sample-rate matrix. Null test residual < −120 dBFS for linear modules." Spec: "Frequency response within ±0.1 dB and ±2° phase of target across 20 Hz–20 kHz"; "Alias products below −80 dBFS with oversampling on".

Tasks: T1 Math/Denormals + sample-rate test matrix fixture; T2 Smoother exponential + zipper test; T3 Biquad RBJ + analytic evaluate; T4 Matched biquads; T5 SVF; T6 OnePole/AllPass tests; T7 FIR design + FIRFilter; T8 Linkwitz–Riley; T9 Detector + EnvelopeFollower; T10 GainComputer + Lookahead + SidechainFilter; T11 FractionalDelayLine + TempoSync; T12 Oversampler; T13 Waveshapers + ADAA; T14 TapeHysteresis + TubeTransfer; T15 Correlation + Spectrum; T16 `mm_render --module` for every module + Python matrix; T17 CPU micro-benchmarks + Report.

### M2 Channel essentials — Channel EQ, Channel Compressor, Gate

Exit criteria: "Frequency response within ±0.1 dB / ±2°; attack/release within 10%; ratio within 0.1; CPU within budget table." Spec phase 2 exit: "null tests against reference transfer curves within 0.1 dB." Null test at flat settings < −120 dBFS (EQ), < −90 dBFS (dynamics at 1:1).

Tasks:
- T1 ADR-0003 EQ band architecture (7 bands: HP/LP 6–48 dB/oct as cascaded matched biquads, 2 shelves, 3 bells; analog-Q = proportional Q; optional 2× OS; 64-bit). `plugins/channel_eq/params.json`: per band `b<n>_enabled, b<n>_type, b<n>_freq (20–20000 Hz, skew 0.25), b<n>_gain_db (−18..+18), b<n>_q (0.1–10, skew 0.3)`, `hp_slope`, `lp_slope`, `q_mode {Digital, Analog}`, `oversample {Off, 2x}`, `analyser {Off, Pre, Post, Pre+Post}`.
- T2 ChannelEqProcessor: band chain of `Biquad<double>`, coefficient update on smoothed freq/gain/q per block (recompute when any changed > epsilon), OS 2× via `Oversampler`, latency reported. Tests: `test_eq_band_response` (each type at 5 freqs × 3 gains × 2 Q, 4 SRs, ±0.1 dB/±2° vs `evaluate` of the design), `test_eq_null_flat` (< −120 dBFS), `test_eq_param_extremes`, `test_eq_cpu` (< 0.5%).
- T3 ChannelEqEditor: `CurveDisplay` with draggable nodes (drag freq/gain, scroll Q, hold to solo band), `SpectrumDisplay` pre/post with peak hold behind curve, band control strip (mockup board 1 layout: band cards "1 · LOW SHELF · 120 Hz · 0.0 · Q 0.7"), footer "Latency 0 ms · 2× OS off". Accent #5B9DE8.
- T4 ADR-0004 compressor topology (feed-forward and feedback selectable; feedback uses one-sample delayed output into detector; program-dependent release = two-stage 60 ms/600 ms blend by crest). `plugins/channel_comp/params.json`: `threshold_db (−60..0)`, `ratio (1..20 + Inf as 21 displayed ∞)`, `attack_ms (0.1–100, skew 0.3)`, `release_ms (10–2000, skew 0.3)`, `auto_release`, `knee_db (0–24)`, `detector {Peak, RMS}`, `topology {FeedForward, Feedback}`, `mix_pct (0–100)`, `sc_hp_hz (20–500)`, `ext_key`, `makeup_db (−12..+24)`, `auto_makeup`, `stereo_link_pct`.
- T5 ChannelCompProcessor: `Detector → SidechainFilter → EnvelopeFollower → GainComputer`, sidechain bus, mix. Tests: `test_comp_ratio` (static tones at −30/−20/−10 dB, ratio within 0.1 for 2,4,8,20,∞), `test_comp_attack_release` (tone-burst, 10%→90% timing within 10% at 1, 10, 100 ms attack; 50, 300, 1000 ms release), `test_comp_knee` (gain at threshold ±knee/2 matches formula within 0.1 dB), `test_comp_null_unity` (ratio 1:1 < −90 dBFS), `test_comp_sidechain_ext`, `test_comp_mono_stereo_link`, `test_comp_cpu` (< 0.4%).
- T6 ChannelCompEditor: transfer curve with live input dot, GR history under curve, threshold draggable on history, GR meter; mockup board 6 layout minus prompt bar. Accent #E0B45C.
- T7 ADR-0005 gate hysteresis (open at threshold, close at threshold − 3 dB hysteresis parameter 0–12 dB). `plugins/gate/params.json`: `threshold_db`, `range_db (−80..0)`, `attack_ms (0.01–100)`, `hold_ms (0–2000)`, `release_ms (1–5000)`, `ratio (1–100, expander)`, `mode {Gate, Expander}`, `hysteresis_db (0–12)`, `sc_hp_hz`, `sc_lp_hz`, `lookahead_ms (0–5)`, `ext_key`, `listen_sc`.
- T8 GateProcessor + tests: `test_gate_open_close_times` (within 10%), `test_gate_hold`, `test_gate_range_floor` (closed gain = range ±0.1 dB), `test_gate_hysteresis_no_chatter` (tone at threshold ±1 dB with 5 Hz AM: ≤ 1 transition per AM cycle), `test_gate_lookahead_latency` (reported == measured), `test_gate_expander_ratio`, `test_gate_cpu` (< 0.3%).
- T9 GateEditor: waveform history with open/closed shading, listen-to-removed button.
- T10 Shared stability fuzz test `tests/plugins/test_stability.py` (every param at audio rate on noise, SR/buffer changes mid-stream, 1 h in nightly CI, 2 min on push) applied to all four plugins so far. Presets (≥ 8 per plugin). Report.

### M3 Colour and space — Saturation, Delay, Reverb

Exit criteria: "Saturation (aliasing < −80 dBFS at 19 kHz, 4× OS), Delay, Reverb (FDN + Dattorro plate). Reverb decay time measured within 10% of labelled RT60." CPU: Saturation < 1.0%, Delay < 0.5%, Reverb < 2.0%.

Tasks:
- T1 ADR-0006 saturation stages (character = Tape {hysteresis + head-bump shelf +2 dB @ 60 Hz + HF roll-off}, Tube {TubeTransfer + even-order bias}, Transistor {asym tanh}, Clip {ADAA soft clip}); 4× fixed OS; `params.json`: `drive_db (0–36)`, `character`, `bias (−1..1)`, `hf_rolloff_hz (2k–20k, Off)`, `mix_pct`, `output_db`.
- T2 SaturationProcessor + tests: `test_sat_aliasing_19k` (< −80 dBFS below 19 kHz at full drive, 4 SRs), `test_sat_harmonic_balance` (bias → even/odd ratio monotonic), `test_sat_latency_reported`, `test_sat_null_zero_drive` (< −90 dBFS with mix 0), `test_sat_cpu` (< 1.0%).
- T3 SaturationEditor: large drive knob, harmonic spectrum even/odd colouring, character tabs with one-line descriptions.
- T4 ADR-0007 delay topology (dual `FractionalDelayLine`, ping-pong cross-feed, feedback path SVF LP/HP, LFO modulation, ducking via `EnvelopeFollower` on dry). `params.json`: `time_l_ms (1–4000)`, `time_r_ms`, `link_lr`, `sync {Off, 1/1 … 1/32, dotted, triplet}`, `feedback_pct (0–120 clamp with soft limit)`, `ping_pong`, `fb_lp_hz`, `fb_hp_hz`, `mod_depth_ms (0–10)`, `mod_rate_hz (0.05–5)`, `mix_pct`, `duck_amount_db`, `duck_release_ms`.
- T5 DelayProcessor + tests: `test_delay_time_accuracy` (echo at labelled ms ± 0.05 ms, 4 SRs), `test_delay_tempo_sync` (120 BPM 1/8 = 250 ms), `test_delay_feedback_decay` (tap n amplitude = fb^n ± 0.2 dB), `test_delay_ping_pong_alternation`, `test_delay_no_zipper_on_time_change` (crossfade/slew: no click > −40 dBFS), `test_delay_cpu`.
- T6 DelayEditor: tap display draggable with tempo grid snap.
- T7 ADR-0008 reverb design (FDN 16 lines Hadamard, per-line damping one-pole, size scales delay lengths (mutually prime), diffusion via 4 input all-passes; Dattorro plate for Plate; early reflections tapped delay; RT60 from feedback gain g = 10^(−3·L/(RT60·fs))). `params.json`: `algorithm {Room, Hall, Plate, Chamber}`, `predelay_ms (0–250)`, `decay_s (0.1–20, skew 0.3)`, `size_pct`, `damping_pct`, `diffusion_pct`, `low_cut_hz`, `high_cut_hz`, `early_late_pct`, `mix_pct`, `width_pct`.
- T8 ReverbProcessor + tests: `test_reverb_rt60` (Schroeder backward integration of impulse response, measured T60 within 10% of label at 0.5/2/8 s, 4 algorithms, 4 SRs), `test_reverb_predelay`, `test_reverb_stability_long_decay` (20 s, no growth: energy decreasing monotonic in 1 s windows), `test_reverb_mono_input`, `test_reverb_cpu` (< 2.0%).
- T9 ReverbEditor: decay envelope display draggable (decay length), teal accent.
- T10 Presets, stability fuzz on the three, listening notes template, Report.

### M4 Stereo and vocal — De-esser, Stereo Imager

Exit criteria: "De-esser, Stereo Imager. Mono-compatibility and correlation tests." CPU < 0.4% each; De-esser latency 0–3 ms.

Tasks:
- T1 De-esser `params.json`: `freq_hz (2k–12k)`, `threshold_db`, `range_db (0–24)`, `mode {Band, Split}`, `width_oct (0.2–2)`, `listen`, `lookahead_ms (0–3)`. Processor: Split = LR4 crossover + dynamics on the high band; Band = SVF band-pass detector driving a dynamic bell (gain = −GR). Tests: `test_deesser_only_band_reduced` (1 kHz tone untouched ±0.05 dB while 6 kHz burst reduced), `test_deesser_range_cap`, `test_deesser_split_sum_flat` (threshold +∞: null < −100 dBFS), `test_deesser_lookahead_latency`, `test_deesser_cpu`.
- T2 De-esser editor: waveform with sibilant regions highlighted, listen button.
- T3 Imager `params.json`: `xover_low_hz (40–500)`, `xover_high_hz (1k–8k)`, `width_low/mid/high_pct (0–200)`, `pan (−100..100)`, `mono_below_hz (Off, 20–500)`, `output_db`. Processor: M/S encode → LR4 3-band split → per-band side gain → decode; mono-below = side high-passed. Tests: `test_imager_width_0_is_mono` (correlation = 1.0 ± 0.001), `test_imager_width_100_null` (< −120 dBFS), `test_imager_width_200_side_gain` (side +6.02 dB ± 0.1), `test_imager_mono_below` (side energy below f < −60 dB), `test_imager_mono_sum_flat` (L+R unchanged within ±0.1 dB for any width: mono compatibility), `test_imager_correlation_meter_accuracy` (known-correlation noise pairs ±0.02), `test_imager_cpu`.
- T4 Imager editor: goniometer + correlation bar, width as Lissajous spread, mono-below line on small spectrum; violet accent.
- T5 Presets, fuzz, Report.

### M5 Mastering — LP FIR engine, LP-EQ, Multiband, Bus comp, M/S, Soft clipper, True-peak limiter

Exit criteria: "EBU Tech 3341/3342 vectors within ±0.1 LU and ±0.3 dBTP; limiter never exceeds ceiling by more than 0.1 dBTP on the test set." Plus per-plugin response/dynamics/CPU criteria from the Global Constraints.

Tasks:
- T1 ADR-0011 linear-phase engine: `fir::PartitionedConvolver` (uniform partitions = host block rounded to pow2, FFT via juce::dsp::FFT, 64-bit), `fir::LinearPhaseDesign` (frequency-sampling of the desired magnitude from the band set, Kaiser window, taps 2048–16384, latency = (N−1)/2 reported). Tests: `test_convolver_matches_direct` (< −120 dBFS vs direct convolution), `test_lp_design_response` (±0.1 dB above the resolution floor f_res = fs/N ×2; documented), `test_lp_latency_reported`.
- T2 `LoudnessMeter` (K-weighting, 400 ms momentary, 3 s short-term, gated integrated −70 LUFS absolute / −10 LU relative, LRA per Tech 3342) + `TruePeakMeter` (4× polyphase per BS.1770-4 Annex 2) in `core/metering`. Fixtures: EBU Tech 3341/3342 vectors (download script with sha256, stored in `tests/fixtures/ebu/`, not vendored if licence forbids; cached in CI). Tests: `test_lufs_ebu_3341` (every vector ±0.1 LU), `test_lra_ebu_3342` (±1 LU per Tech 3342 tolerance), `test_truepeak_3341_tp` (±0.3 dBTP), cross-check against pyloudnorm.
- T3 Linear-phase EQ: 8 bands, per-band `phase {Minimum, Linear}`, `fir_length {2048, 4096, 8192, 16384}`, `ms_mode per band {Stereo, Mid, Side}`. Minimum-phase bands reuse Channel EQ chain; linear bands composed into one FIR. Tests: response ±0.1 dB (above resolution floor), phase linear within ±2° (group delay flat), null flat < −120 dBFS, CPU < 2.5%, latency readout matches. Editor shows latency cost as FIR lengthens.
- T4 Multiband compressor: 3–5 bands, LR4 crossovers (phase-coherent sum), per-band dynamics, solo, global mix. Tests: `test_mb_crossover_sum_flat` (all bands 1:1 → null < −90 dBFS; phase-coherent), `test_mb_per_band_ratio` (within 0.1), `test_mb_band_isolation` (tone in band 1 produces no GR in band 3), CPU < 1.5%. Editor: spectrum with draggable crossovers, GR shaded per band.
- T5 Bus compressor: stepped `ratio {2,4,10}`, `attack {0.1,0.3,1,3,10,30}`, `release {0.1,0.3,0.6,1.2,Auto}`, feed-forward VCA, stereo-linked detector, optional 2× OS, sidechain HP, mix. Tests: attack/release within 10% of labelled, ratio within 0.1, null at 1:1-equivalent (threshold max) < −90 dBFS, CPU < 0.5%. Editor: VU-style GR needle (300 ms ballistics), mockup board 5 as the analogue skin.
- T6 M/S processor: mid/side gain, 2-band EQ per channel (matched biquads), width, mono-below, solo. Tests: encode/decode null < −120 dBFS, width = imager semantics, mono-sum flat, CPU < 0.5%.
- T7 Soft clipper: ADAA clipper at 4×/8× OS, ceiling, knee, true-peak-aware ceiling (post-OS peak), delta listen, mix. Tests: `test_clip_never_exceeds_ceiling` (sample peak at OS rate ≤ ceiling + 0.01 dB), `test_clip_aliasing_19k` (< −80 dBFS), `test_clip_knee_curve`, CPU < 1.5%.
- T8 ADR-0012 limiter design: 4× OS true-peak detection, lookahead 0–10 ms with max-envelope + smoothing (attack = lookahead), dual-stage release (fast 10–50 ms + slow 100–1000 ms, Auto by crest), character {Transparent, Punchy, Loud} = release/knee presets, stereo link %, ceiling in dBTP, dither out (TPDF 16/24). Tests: `test_limiter_ceiling_never_exceeded` (true-peak ≤ ceiling + 0.1 dBTP on the test set: EBU vectors + 20 synthetic worst-cases: 0.45·fs tone bursts, square waves, inter-sample peak vectors), `test_limiter_release_times` (10%), `test_limiter_null_below_threshold` (< −90 dBFS), `test_limiter_latency_reported`, `test_limiter_aliasing`, CPU < 2.5%. Editor: GR history "hangs from the ceiling" in gold (mockup board 2), true-peak overs as red ticks, LUFS-I/TP/LRA readouts.
- T9 Presets (mastering chain), fuzz on all seven, chain-order integration test (typical chain renders within budget), Report.

### M6 Delivery — Loudness meter + dither, preset libraries, shared GUI on every plugin, installer, licensing stub, docs

Exit criteria (CLAUDE.md): "Loudness meter + dither, preset libraries, shared GUI chrome on every plugin, installer (Full/Lite), licensing stub, docs." Spec: meter/dither CPU < 0.5%; presets shared browser with tags and "similar presets"; chain templates; MIDI learn + macros; full keyboard navigation and screen-reader labels; installer Lite under 100 MB.

Tasks:
- T1 Loudness meter & dither plugin: target presets (Spotify −14, Apple −16, YouTube −14, EBU R128 −23), LUFS M/S/I, LRA, true-peak, 3-min history ribbon, compliance badge, export report (JSON/CSV to user-chosen path, off the audio thread), dither TPDF / noise-shaped (ITU-R 468 weighted 2nd-order shaper) at 16/24-bit. Tests: EBU vectors (reuse M5 core tests through the plugin), `test_dither_spectrum` (TPDF noise floor at −93 + 4.77 dB for 16-bit ±0.5 dB; noise-shaped curve shape), `test_dither_bit_depth` (output quantised exactly to 16/24 bits), CPU < 0.5%. Mockup board 3.
- T2 Preset browser overlay: search, tags (genre/instrument/task), favourites, "similar presets" (nearest neighbour on normalised parameter vector), chain templates (save/recall a chain across plugins via a shared JSON file in the user preset folder; each plugin applies its own section). Tests: preset load/save round-trip for every factory preset of every plugin (ctest over `presets/*.mmpreset`), tag filter unit tests.
- T3 Accessibility + keyboard: tab order, arrow adjust, `juce::AccessibilityHandler` labels from params.json names/units; MIDI learn + 8 macro knobs stored in plugin state. Tests: every parameter has an accessible name (ctest walks the editor), MIDI CC mapping round-trip.
- T4 Light theme + per-family accent audit across 16 plugins; resize 50–200% screenshot test (headless render at 3 scales, no layout overflow via bounds assertions).
- T5 Installer: macOS `pkgbuild` + notarisation hooks (signing identities as CI secrets; unsigned local builds), Windows Inno Setup, Linux tar/AppImage; Full vs Lite CMake option `MM_BUNDLE_MODELS`; Lite size test < 100 MB. Licensing stub: offline key file `~/.mm/licence.json` validated by an Ed25519 signature check (stub accepts a documented dev key), one licence covers all formats.
- T6 Docs: user manual per plugin generated from params.json (`tools/gen_docs.py`), CHANGELOG, README with the Commands section. Report + beta-round checklist (manual DAW pass list from the spec: Pro Tools, Logic, Ableton, Cubase, Reaper, Studio One, Bitwig).

### M7 AI layer

Exit criteria (CLAUDE.md): "feature extractor, schema generator, llama.cpp host (shared singleton, worker thread), prompt bar + proposal diff UI, `/ml` pipeline (data tooling, MLX and PyTorch training scripts, eval suite implementing the spec's release gate, GGUF export), Full installer bundling the model. The first model candidate ships only when it beats the rules-based mapper and the 7B zero-shot baseline on the eval suite." Release gate (spec): schema validity 100%; parameter accuracy within 1.5 dB / 25% time / 20% ratio on 85% of held-out; direction accuracy 95%; listening test 60%+ win/tie; refusal 90%+; beats both baselines; p95 < 2 s; regression ≤ 2 points.

Tasks:
- T1 ADR-0013 AI architecture (model host singleton, worker thread, request queue, cancellation, memory budget, schema version string `mm-schema-1`). ADR-0014 fine-tuning pipeline.
- T2 `ai/features/FeatureExtractor`: from the last 4–8 s ring buffer (off the audio thread: audio thread pushes into a FIFO, worker computes): integrated + short-term LUFS, crest factor, peak dBTP, spectral centroid and tilt, energy in 6 bands, transient density/s, stereo width and correlation, source class {kick, snare, bass, vocal, guitar, keys, full_mix, other} with confidence (rules-based v1 from band energies + transient density; ONNX classifier slot for later). Tests: synthetic stems with known features (sine → centroid; clicks → density), determinism (same audio → identical block), timing < 50 ms for 8 s at 48 k.
- T3 Schema generator extension: `gen_params.py --gbnf` emits a GBNF grammar per plugin from `tool_schema.json` (only `ai: true` params, unit-bearing names like `attack_ms`, numeric ranges); build fails if a param lacks `unit` while `ai: true`. Tests: grammar accepts the spec's example output, rejects out-of-range / unknown keys / non-JSON.
- T4 `ai/llm/ModelHost`: embedded llama.cpp (FetchContent, SHA-pinned, Metal on macOS, CPU fallback; DirectML via ONNX only for neural DSP), singleton loaded by the first plugin that needs it, worker thread, 2 k context, grammar-constrained decoding, strict JSON validation + range clamp + unknown-key rejection, refuses model if `schema_version` in GGUF metadata ≠ `mm-schema-1`. Prompt templates in `ai/prompts/` (system prompt, feature block, current params, history of 3 turns). Tests: with a tiny stub GGUF in CI (or the real 0.5 B model cached): valid JSON 100/100, p95 latency logged, cancellation, two plugins share one host (ctest spawns two processors).
- T5 UI: `PromptBar` in the header (keystroke-invoked, typed input; spoken input deferred to whisper.cpp follow-up), proposal as ghost knob positions + current→proposed table (mockup board 6), rationale line, confidence, "Why?" affordance, Apply/Revert, preview gain-matched, history, permanent "Offline · on this machine" badge, model version in settings. Opt-in local logging of accepted/rejected proposals to `~/.mm/telemetry/*.jsonl`.
- T6 `/ml` pipeline: `ml/data/` (JSONL contract, drafts/ with the first 100-per-plugin batch written by Claude in the spec's contract: `source_class, prompt, target_params, rationale, stem_id` placeholder; augmentation script for paraphrases incl. French and Kinyarwanda; public-dataset mappers for MixAssist/SocialFX/Text2FX; split 80/10/10 by stem), `ml/train/train_mlx.py` (0.5 B LoRA on Mac) and `ml/train/train_torch.py` (QLoRA rank 16–32 on 24 GB GPU, loss masked to JSON output, curriculum single-param → full → multi-turn, pinned seeds and versions), `ml/eval/` (the gate above as pytest: schema validity, MAE in perceptual units, direction accuracy, refusal set, baselines: rules-based mapper `ml/eval/rules_mapper.py` and Qwen2.5-7B zero-shot, speed p95, regression vs shipped model hash, listening-test sheet generator), `ml/export/` (merge → safetensors → GGUF F16 → Q4_K_M with schema-version metadata).
- T7 Installer: Full bundles the GGUF inside the bundle/resource folder (`MM_BUNDLE_MODELS=ON`), Lite omits it; size test 1.5–2.5 GB Full, < 100 MB Lite; code-signing with large payloads verified in CI (signing optional locally).
- T8 Gate run on the first candidate, report with metrics table vs baselines, Report. Ships only if every gate passes; otherwise REVIEW.md says which gate failed and the next step (1.5 B candidate or more gold data).
- T9 (deferred unless the reviewer pulls it in) Neural DSP track from the spec: ONNX Runtime statically linked (FetchContent, SHA-pinned), CoreML/DirectML/CPU providers, and a first denoise plugin on DeepFilterNet. Not in CLAUDE.md's M7 list, so it is scheduled after M7 as phase 7+ together with the spec's optional plugins (pitch correction, vocal rider, restoration, reference-match EQ, transient shaper, convolution reverb, stem separation, Atmos panner).

## ADR list

| ADR | Title | Milestone |
| --- | --- | --- |
| 0001 | Build system: CMake + FetchContent SHA pins, formats, AAX stub, sanitizers, CI matrix | M0 |
| 0002 | Parameter model: params.json → generated Params.h + tool schema; reserved params; smoothing; presets as ValueTree XML + JSON meta | M0 |
| 0003 | Channel EQ band architecture and analog-Q | M2 |
| 0004 | Compressor topologies and program-dependent release | M2 |
| 0005 | Gate hysteresis and lookahead | M2 |
| 0006 | Saturation character stages and fixed 4× OS | M3 |
| 0007 | Delay topology, time-change slew, ducking | M3 |
| 0008 | Reverb: FDN-16 + Dattorro plate, RT60 mapping | M3 |
| 0009 | De-esser topology (band / split), detector, lookahead, listen | M4 |
| 0010 | Stereo imager topology (three-band M/S width, mono-below, pan, correlation) | M4 |
| 0011 | Linear-phase FIR engine (partitioned convolution, design, latency) | M5 |
| 0012 | True-peak limiter design | M5 |
| 0013 | AI runtime: shared model host, worker thread, grammar-constrained JSON, schema versioning | M7 |
| 0014 | Fine-tuning pipeline, data contract, release gate | M7 |

## Risks and decisions already taken

- **AAX SDK absent** → format stubbed; `MM_AAX_SDK_PATH` enables it. Documented in ADR-0001 and every REVIEW.md until provided.
- **No Xcode, Command Line Tools only** → AU and Standalone build with CLT; `auval` is in `/usr/bin` and works without Xcode. pluginval covers AU validation in CI.
- **Matched (Vicanek) biquads vs analog prototype** → the ±0.1 dB test compares the implementation to its own discrete transfer function; the analog-match tolerance is reported separately and raised as a spec question in M1 REVIEW.md.
- **Linear-phase EQ ±0.1 dB** is only meaningful above the FIR's frequency resolution; the test states the floor explicitly (raised as spec question in M5).
- **EBU test vectors licence** → fetched by script with sha256 at test time (dev machine and CI cache), not vendored, unless licence permits.
- **GitHub remote** → CI "green" needs a repository; the executor creates a private repo with `gh repo create` under the user's account unless told a different org. Listed as a reviewer question in M0's REVIEW.md.
- **Listening gate** (blind A/B with two engineers) cannot be automated; each milestone's REVIEW.md ships a listening sheet and marks the gate "awaiting engineers".
