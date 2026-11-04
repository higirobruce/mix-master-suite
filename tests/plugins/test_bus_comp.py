"""Bus compressor (M5 Task 5): stepped attack / release within 10 % on DC steps, stepped ratios within 0.1 on DC
levels, null at the top threshold, click-free switches, oversampling latency, sidechain high-pass, base checks, CPU."""
from __future__ import annotations

import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "bus_comp"
ATTACKS = [0.1, 0.3, 1.0, 3.0, 10.0, 30.0]
RELEASES = [100.0, 300.0, 600.0, 1200.0]
RATIOS = [2.0, 4.0, 10.0]
BASE = {"threshold_db": -20.0, "ratio": 1, "attack": 2, "release": 0, "makeup_db": 0.0, "mix_pct": 100.0, "sc_hp_hz": 0.0, "oversample": 0}


def dc(level_db: float, sr: int, seconds: float = 1.0) -> np.ndarray:
    return np.full((int(seconds * sr), 2), 10 ** (level_db / 20), dtype=np.float32)


@pytest.mark.parametrize("ratio_index,ratio", list(enumerate(RATIOS)))
def test_bus_comp_ratio(ratio_index, ratio, record_property):
    """DC levels -12 / -6 / 0 dBFS above the -20 threshold (a peak detector reads DC exactly): slope = 1 / ratio."""
    sr = 48000
    outs = []
    for level in (-12.0, -6.0, 0.0):
        y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ratio=ratio_index), input=dc(level, sr))
        assert rep["nan_count"] == 0
        outs.append(20 * np.log10(np.mean(np.abs(y[int(0.6 * sr):, 0]))))
    for a, b in ((0, 1), (1, 2)):
        slope = (outs[b] - outs[a]) / 6.0
        record_property(f"ratio_{ratio}_measured", float(1 / slope))
        assert abs(1 / slope - ratio) < 0.1


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("attack_index,attack_ms", list(enumerate(ATTACKS)))
def test_bus_comp_attack(sr, attack_index, attack_ms, record_property):
    """DC step 0.01 -> 1.0 (-20 threshold, 4:1): 10-90 % of the 15 dB reduction within 10 % + one sample."""
    n = int(0.6 * sr)
    t = np.arange(n) / sr
    x = np.where(t >= 0.1, 1.0, 0.01).astype(np.float32)
    x = np.stack([x, x], axis=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, attack=attack_index, release=0), input=x)
    assert rep["nan_count"] == 0
    g = 20 * np.log10(np.abs(y[:, 0]) / (np.abs(x[:, 0]) + 1e-12) + 1e-12)
    on = g[int(0.1 * sr):]  # from the step sample itself: a 0.1 ms attack is five samples long
    gr = -np.mean(on[-len(on) // 5:])
    assert abs(gr - 15.0) < 0.1
    atk = (np.argmax(on <= -0.9 * gr) - np.argmax(on <= -0.1 * gr)) / sr * 1000
    record_property(f"attack_{attack_ms}_measured_{sr}", float(atk))
    # 10 % plus two samples: each threshold crossing quantises to a sample
    assert abs(atk - attack_ms) <= 0.1 * attack_ms + 2000.0 / sr


@pytest.mark.parametrize("release_index,release_ms", list(enumerate(RELEASES)))
def test_bus_comp_release(release_index, release_ms, record_property):
    sr = 48000
    n = int((0.6 + 4 * release_ms / 1000) * sr)
    t = np.arange(n) / sr
    x = np.where((t >= 0.1) & (t < 0.4), 1.0, 0.01).astype(np.float32)
    x = np.stack([x, x], axis=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, attack=0, release=release_index), input=x)
    assert rep["nan_count"] == 0
    g = 20 * np.log10(np.abs(y[:, 0]) / (np.abs(x[:, 0]) + 1e-12) + 1e-12)
    on = g[int(0.1 * sr) + 2:int(0.4 * sr) - 2]
    off = g[int(0.4 * sr) + 2:]
    gr = -np.mean(on[-len(on) // 5:])
    rel = (np.argmax(off >= -0.1 * gr) - np.argmax(off >= -0.9 * gr)) / sr * 1000
    record_property(f"release_{release_ms}_measured", float(rel))
    assert abs(rel - release_ms) <= 0.1 * release_ms + 1000.0 / sr


def test_bus_comp_auto_release_recorded(record_property):
    sr = 48000
    n = int(3.0 * sr)
    t = np.arange(n) / sr
    x = np.where((t >= 0.1) & (t < 0.4), 1.0, 0.01).astype(np.float32)
    x = np.stack([x, x], axis=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, attack=0, release=4), input=x)
    assert rep["nan_count"] == 0
    g = 20 * np.log10(np.abs(y[:, 0]) / (np.abs(x[:, 0]) + 1e-12) + 1e-12)
    off = g[int(0.4 * sr) + 2:]
    gr = -np.mean(g[int(0.3 * sr):int(0.4 * sr) - 2])
    rel = (np.argmax(off >= -0.1 * gr) - np.argmax(off >= -0.9 * gr)) / sr * 1000
    record_property("auto_release_measured_ms", float(rel))
    assert 50.0 < rel < 1500.0


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_bus_comp_null_at_max_threshold(sr):
    x = signals.noise(sr, 1.0, seed=2, amp_db=-6)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, threshold_db=15.0), input=x)
    assert rep["latency_samples"] == 0
    assert measure.residual_dbfs(y, x) < -90.0


def test_bus_comp_switches_no_click(record_property):
    """Attack 0.1 -> 30 ms, ratio 2 -> 10 and release 0.1 s -> Auto switched during 12 dB of reduction on a tone:
    no sample step above 1.5x the static output's own; the 2x switch re-reports the latency and does not click."""
    sr = 48000
    x = signals.sine(500.0, sr, 3.0, amp_db=-6)
    params = dict(BASE, threshold_db=-24.0, ratio=0, attack=0, release=0)
    auto = [{"param": "attack", "t_s": 1.0, "value": 5}, {"param": "ratio", "t_s": 1.5, "value": 2}, {"param": "release", "t_s": 2.0, "value": 4}]
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x, automation=auto)
    assert rep["nan_count"] == 0
    static = float(np.max(np.abs(np.diff(y[int(0.6 * sr):int(0.95 * sr), 0]))))
    step = float(np.max(np.abs(np.diff(y[int(0.95 * sr):int(2.9 * sr), 0])))) / static
    record_property("switch_step_ratio", step)
    assert step < 1.5
    y2, rep2 = render_plugin(PLUGIN, sr=sr, params=dict(params, oversample=1), input=x)
    assert rep2["latency_samples"] > 0
    imp = signals.impulse(sr, 0.2, channels=2)
    yi, repi = render_plugin(PLUGIN, sr=sr, params=dict(BASE, threshold_db=15.0, oversample=1), input=imp)
    assert int(np.argmax(np.abs(yi[:, 0]))) == repi["latency_samples"]
    y3, rep3 = render_plugin(PLUGIN, sr=sr, params=params, input=x, automation=[{"param": "oversample", "t_s": 1.5, "value": 1}])
    assert rep3["nan_count"] == 0
    step3 = float(np.max(np.abs(np.diff(y3[int(1.3 * sr):int(2.9 * sr), 0])))) / static
    record_property("oversample_switch_step_ratio", step3)
    assert step3 < 1.5
    del y2


def test_bus_comp_sc_hp():
    """A 40 Hz tone with the detector high-passed at 120 Hz causes no reduction; a 1 kHz tone of the same level does."""
    sr = 48000
    for hz, reduced in ((40.0, False), (1000.0, True)):
        x = signals.sine(hz, sr, 1.0, amp_db=-6)
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, sc_hp_hz=120.0, ratio=2), input=x)
        gain = measure.level_at_db(y[:, 0], sr, hz, start_s=0.6) - measure.level_at_db(x[:, 0], sr, hz, start_s=0.6)
        assert (gain < -6.0) == reduced, f"{hz} Hz: gain {gain:.2f} dB"


def test_bus_comp_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=32.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=dict(BASE, threshold_db=15.0)))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=dict(BASE, threshold_db=15.0)))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
