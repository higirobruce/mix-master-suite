"""Multiband compressor (M5 Task 4): phase-coherent crossover sum, per-band ratio, band isolation, attack / release,
crossover ordering, solo / bypass, base checks, CPU budget."""
from __future__ import annotations

import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module, render_plugin
from tests.plugins import base_checks

PLUGIN = "multiband_comp"
XOVER = {"xover_1_hz": 120.0, "xover_2_hz": 600.0, "xover_3_hz": 2500.0, "xover_4_hz": 8000.0}
OPEN = {f"b{b}_threshold_db": 0.0 for b in range(1, 6)} | {f"b{b}_ratio": 1.0 for b in range(1, 6)}
BAND_TONES = {3: [60.0, 1000.0, 6000.0], 4: [60.0, 300.0, 1200.0, 6000.0], 5: [60.0, 300.0, 1200.0, 4500.0, 14000.0]}
NUM_BANDS_INDEX = {3: 0, 4: 1, 5: 2}


def allpass_reference(x: np.ndarray, sr: int, num_bands: int, xover: dict | None = None) -> np.ndarray:
    """The LR4 all-pass of every active crossover in turn (the sum of every band at 1:1), from the core module."""
    xo = dict(XOVER, **(xover or {}))
    y = x
    for i in range(1, num_bands):
        y, _ = render_module("lr_crossover", sr=sr, params={"freq_hz": xo[f"xover_{i}_hz"], "order": 4, "output": 2}, input=y, channels=2)
    return y


def level(y: np.ndarray, sr: int, hz: float, start_s: float = 0.6) -> float:
    return measure.level_at_db(y[:, 0], sr, hz, start_s=start_s)


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("num_bands", [3, 4, 5])
def test_mb_crossover_sum_flat(sr, num_bands, record_property):
    x = signals.noise(sr, 1.0, seed=4)
    params = dict(OPEN, **XOVER, num_bands=NUM_BANDS_INDEX[num_bands])
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0 and rep["latency_samples"] == 0
    res = measure.residual_dbfs(y, allpass_reference(x, sr, num_bands))
    record_property(f"sum_residual_{num_bands}_{sr}", res)
    assert res < -100.0


# Per-band geometry with the tone at least two octaves inside its band (an LR4 edge leaks -24 dB at one octave, which
# would read as a lower ratio): (num_bands index, crossovers, tone Hz)
RATIO_GEOMETRY = {
    1: (0, {"xover_1_hz": 400.0, "xover_2_hz": 2000.0}, 60.0),
    2: (0, {"xover_1_hz": 40.0, "xover_2_hz": 2000.0}, 300.0),
    3: (0, {"xover_1_hz": 40.0, "xover_2_hz": 200.0}, 2000.0),
    4: (1, {"xover_1_hz": 40.0, "xover_2_hz": 200.0, "xover_3_hz": 1000.0}, 8000.0),
    5: (2, {"xover_1_hz": 40.0, "xover_2_hz": 200.0, "xover_3_hz": 1000.0, "xover_4_hz": 4000.0}, 16000.0),
}


@pytest.mark.parametrize("band", [1, 2, 3, 4, 5])
@pytest.mark.parametrize("ratio", [2.0, 4.0, 8.0, 20.0])
def test_mb_per_band_ratio(band, ratio, record_property):
    """A tone two octaves inside the band at three levels above its threshold, the band soloed (the neighbouring
    bands' uncompressed stopband residual, -66 .. -80 dB, would otherwise bend the slope by up to 0.9 at 20:1): the
    slope gives the ratio within 0.1."""
    sr = 48000
    nb, xo, hz = RATIO_GEOMETRY[band]
    params = {**OPEN, **XOVER, **xo, "num_bands": nb, "detector": 1, f"b{band}_solo": 1}
    params.update({f"b{band}_threshold_db": -20.0, f"b{band}_ratio": ratio, f"b{band}_attack_ms": 1.0, f"b{band}_release_ms": 100.0})
    outs = []
    for in_db in (-12.0, -6.0, 0.0):
        x = signals.sine(hz, sr, 1.0, amp_db=in_db)
        y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
        assert rep["nan_count"] == 0
        outs.append(level(y, sr, hz))
    for a, b in ((0, 1), (1, 2)):
        slope = (outs[b] - outs[a]) / 6.0
        record_property(f"band{band}_ratio_{ratio}", float(1 / slope))
        assert abs(1 / slope - ratio) < 0.1


def test_mb_band_isolation():
    """A loud tone in band 1 must not move band 3's gain: a quiet band-3 tone below its own threshold is unchanged."""
    sr = 48000
    x = signals.sine(60.0, sr, 1.0, amp_db=-6) + signals.sine(1200.0, sr, 1.0, amp_db=-40)
    params = dict(OPEN, **XOVER, num_bands=1, detector=0)
    params.update({"b3_threshold_db": -30.0, "b3_ratio": 20.0, "b1_threshold_db": -60.0, "b1_ratio": 4.0})
    y, _ = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    ref = allpass_reference(x, sr, 4)
    assert abs(level(y, sr, 1200.0) - level(ref, sr, 1200.0)) < 0.1
    assert level(y, sr, 60.0) < level(ref, sr, 60.0) - 10.0  # band 1 is being compressed


@pytest.mark.parametrize("attack_ms,release_ms", [(10.0, 100.0), (100.0, 1000.0)])
def test_mb_attack_release(attack_ms, release_ms, record_property):
    """A 2 kHz tone (band 3 of three, crossovers 40 / 200 Hz) stepping -40 -> 0 dBFS -> -40, peak detector: the
    band's reduction reaches 10 % / 90 % of its final value in the same time as the Channel Compressor on the same
    burst (within 10 % + one cycle). A peak detector on a tone's rectified ripple reads longer than the label for
    either (M2 proved the 10 % gate with DC steps); the multiband must not add to it."""
    sr = 48000
    n = int((0.6 + 4 * release_ms / 1000) * sr)
    t = np.arange(n) / sr
    env = np.where((t >= 0.1) & (t < 0.4), 1.0, 10 ** (-40 / 20))
    tone = np.sin(2 * np.pi * 2000.0 * t) * env
    x = np.stack([tone, tone], axis=1).astype(np.float32)
    xo = {"xover_1_hz": 40.0, "xover_2_hz": 200.0}
    params = {**OPEN, **XOVER, **xo, "num_bands": 0, "detector": 0, "b3_solo": 1}
    params.update({"b3_threshold_db": -20.0, "b3_ratio": 4.0, "b3_attack_ms": attack_ms, "b3_release_ms": release_ms})
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    # the band at 1:1 through the same tree: the gain is read against it, not the raw input, so the crossover's own
    # transient at the burst edges does not read as compression
    yref, _ = render_plugin(PLUGIN, sr=sr, params={**OPEN, **XOVER, **xo, "num_bands": 0, "b3_solo": 1}, input=x)
    comp_params = {"threshold_db": -20.0, "ratio": 4.0, "knee_db": 6.0, "attack_ms": attack_ms, "release_ms": release_ms,
                   "detector": 0, "mix_pct": 100.0, "makeup_db": 0.0, "stereo_link_pct": 100.0}
    yc, _ = render_plugin("channel_comp", sr=sr, params=comp_params, input=x)
    win = int(sr / 2000.0)  # one cycle

    def timings(out, ref):
        s_ = np.abs(out[:, 0])
        e = np.array([np.max(s_[i:i + win]) for i in range(0, len(s_) - win, win)])
        xe = np.array([np.max(np.abs(ref[i:i + win, 0])) for i in range(0, len(ref) - win, win)])
        g = 20 * np.log10(e / (xe + 1e-12) + 1e-12)
        tw = np.arange(len(g)) * win / sr
        on = g[(tw >= 0.1 + 2 * win / sr) & (tw < 0.4 - 2 * win / sr)]
        off = g[tw >= 0.4 + 2 * win / sr]
        gr = -np.mean(on[-len(on) // 5:])
        atk = (np.argmax(on <= -0.9 * gr) - np.argmax(on <= -0.1 * gr)) * win / sr * 1000
        rel = (np.argmax(off >= -0.1 * gr) - np.argmax(off >= -0.9 * gr)) * win / sr * 1000
        return gr, atk, rel

    gr, atk, rel = timings(y, yref)
    grc, atkc, relc = timings(yc, x)
    record_property("attack_ms_measured", float(atk))
    record_property("release_ms_measured", float(rel))
    record_property("attack_ms_channel_comp", float(atkc))
    record_property("release_ms_channel_comp", float(relc))
    assert abs(gr - grc) < 0.5
    assert abs(atk - atkc) <= 0.1 * attack_ms + win / sr * 1000
    assert abs(rel - relc) <= 0.1 * release_ms + win / sr * 1000


def test_mb_crossover_order_enforced():
    """Crossover 2 dragged below crossover 1: it is held a third of an octave above it, and the sum stays the
    all-pass of the clamped pair (no click, no hole)."""
    sr = 48000
    x = signals.noise(sr, 1.0, seed=8)
    params = {**OPEN, **XOVER, "num_bands": 1, "xover_1_hz": 400.0, "xover_2_hz": 200.0}
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    clamped = {"xover_1_hz": 400.0, "xover_2_hz": 400.0 * 2 ** (1 / 3)}
    assert measure.residual_dbfs(y, allpass_reference(x, sr, 4, clamped)) < -100.0


def test_mb_solo_and_bypass():
    sr = 48000
    # three bands, crossovers 400 / 2000: the 60 Hz tone sits 2.7 octaves inside band 1, the 12 kHz tone 2.6 inside band 3
    x = signals.sine(60.0, sr, 1.0, amp_db=-12) + signals.sine(12000.0, sr, 1.0, amp_db=-12)
    params = {**OPEN, **XOVER, "num_bands": 0, "xover_1_hz": 400.0, "xover_2_hz": 2000.0}
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(params, b1_solo=1), input=x)
    assert level(y, sr, 12000.0) < -70.0 and abs(level(y, sr, 60.0) + 12.0) < 0.3
    heavy = dict(params, b3_threshold_db=-40.0, b3_ratio=20.0)
    y, _ = render_plugin(PLUGIN, sr=sr, params=heavy, input=x)
    assert level(y, sr, 12000.0) < -30.0
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(heavy, b3_bypass=1), input=x)
    assert abs(level(y, sr, 12000.0) + 12.0) < 0.3


def test_mb_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=24.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=dict(OPEN, **XOVER), tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=dict(OPEN, **XOVER), tol_dbfs=-80.0, settle_s=0.2))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=dict(OPEN, **XOVER))
    base_checks.check_channel_configs(PLUGIN, params=dict(OPEN, **XOVER))


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))


@pytest.mark.parametrize("hz", [100.0, 1000.0])
def test_mb_num_bands_switch_no_click(hz, record_property):
    """Band count switched 4 -> 5 while a tone runs, all bands 1:1: the old and new trees run side by side and
    crossfade, so no sample step above 1.5x the static output's own (review finding: an instant switch stepped 6-7x)."""
    sr = 48000
    x = signals.sine(hz, sr, 3.0, amp_db=-6)
    params = dict(XOVER, **OPEN, num_bands=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x, automation=[{"param": "num_bands", "t_s": 1.5, "value": 2}])
    assert rep["nan_count"] == 0
    static = float(np.max(np.abs(np.diff(y[int(0.6 * sr):int(1.4 * sr), 0]))))
    step = float(np.max(np.abs(np.diff(y[int(1.4 * sr):int(2.9 * sr), 0])))) / static
    record_property(f"num_bands_switch_step_ratio_{int(hz)}", step)
    assert step < 1.5
