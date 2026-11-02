import numpy as np
import pytest

from tests.core.test_reverb_modules import schroeder_t60
from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "reverb"
ALGORITHMS = ["Room", "Hall", "Plate", "Chamber"]
# wet only, no filtering, late tail only, no damping: the measurement setting for the RT60 gate (ADR-0008)
BASE = {"algorithm": 1, "predelay_ms": 0.0, "decay_s": 2.0, "size_pct": 60.0, "damping_pct": 0.0, "diffusion_pct": 70.0,
        "low_cut_hz": 20.0, "high_cut_hz": 20000.0, "early_late_pct": 0.0, "mix_pct": 100.0, "width_pct": 100.0}


def impulse_response(sr, params, seconds, channels=2):
    x = signals.impulse(sr, seconds, channels=channels)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    return y, rep


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("decay", [0.5, 2.0, 8.0])
@pytest.mark.parametrize("algorithm", ALGORITHMS)
def test_reverb_rt60(sr, decay, algorithm, record_property):
    y, _ = impulse_response(sr, dict(BASE, algorithm=ALGORITHMS.index(algorithm), decay_s=decay), min(decay * 1.6 + 0.5, 14.0))
    t60 = schroeder_t60(y[:, 0] + y[:, 1], sr)
    record_property("t60_measured", float(t60))
    assert abs(t60 - decay) / decay < 0.10


def test_reverb_predelay():
    sr = 48000
    for pre in (0.0, 50.0, 250.0):
        y, _ = impulse_response(sr, dict(BASE, predelay_ms=pre, early_late_pct=100.0), 1.0)
        first = int(np.argmax(np.abs(y[:, 0]) > 1e-4)) / sr * 1000  # first early reflection
        y0, _ = impulse_response(sr, dict(BASE, predelay_ms=0.0, early_late_pct=100.0), 1.0)
        first0 = int(np.argmax(np.abs(y0[:, 0]) > 1e-4)) / sr * 1000
        assert abs((first - first0) - pre) < 1.0


def test_reverb_stability_long_decay():
    # Review Focus 3: decay 20 s, size 100, diffusion 100: after the input stops, 1 s-window energy keeps decreasing
    sr = 48000
    y, _ = impulse_response(sr, dict(BASE, decay_s=20.0, size_pct=100.0, diffusion_pct=100.0), 12.0)
    m = y[:, 0] + y[:, 1]
    windows = [float(np.sum(m[i:i + sr] ** 2)) for i in range(sr, len(m) - sr, sr)]
    assert all(b < a for a, b in zip(windows, windows[1:]))


def test_reverb_mono_input_stereo_tail_and_width():
    sr = 48000
    x = signals.noise(sr, 0.3, seed=9, amp_db=-12.0, channels=1)
    x = np.concatenate([x, np.zeros((int(1.5 * sr), 1), dtype=np.float32)])
    y, rep = render_plugin(PLUGIN, sr=sr, params=BASE, input=x, layout={"in": 1, "out": 2})
    assert rep["nan_count"] == 0
    tail = y[int(0.5 * sr):int(1.5 * sr)]
    corr = np.corrcoef(tail[:, 0], tail[:, 1])[0, 1]
    assert abs(corr) < 0.5  # decorrelated stereo tail from a mono source
    y0, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_pct=0.0), input=x, layout={"in": 1, "out": 2})
    np.testing.assert_allclose(y0[:, 0], y0[:, 1], atol=1e-5)  # width 0: identical channels


def test_reverb_algorithm_switch_click(record_property):
    # Review Focus 3: switching Hall -> Plate mid-tail fades (50 ms) rather than clicks
    sr = 48000
    x = signals.noise(sr, 2.0, seed=10, amp_db=-12.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, decay_s=3.0), input=x, automation=[{"param": "algorithm", "t_s": 1.0, "value": 2}])
    assert rep["nan_count"] == 0
    # 10 ms windows: a reverb tail on noise fluctuates by a few dB per ms by itself; a hard switch between engines
    # of different level would still step by their full level difference in one window
    step = measure.envelope_step_db(y[:, 0], x[:, 0], sr, win_ms=10.0, start_s=0.5)
    record_property("switch_envelope_step_db", step)
    assert step < 3.0


def test_reverb_null_mix_zero():
    sr = 48000
    x = signals.noise(sr, 1.0, seed=11, channels=2)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mix_pct=0.0), input=x)
    assert rep["latency_samples"] == 0
    assert measure.residual_dbfs(y, x) < -120.0


def test_reverb_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=12.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    # a stateful 2 s tail re-derived at a new rate: only a loose match is meaningful (recorded)
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=dict(BASE, decay_s=0.3), tol_dbfs=-20.0, settle_s=0.5))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=dict(BASE, decay_s=0.2))
    base_checks.check_channel_configs(PLUGIN, params=dict(BASE, early_late_pct=100.0, predelay_ms=0.0))


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
