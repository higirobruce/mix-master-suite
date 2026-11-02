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


LEAD_S = 0.35  # the hosted plugin receives its initial parameter values with the first process call, smooths them
               # over 20 ms and glides its line lengths over 200 ms: the impulse waits for all of it to settle


def impulse_response(sr, params, seconds, channels=2):
    x = signals.impulse(sr, seconds, channels=channels)
    x = np.concatenate([np.zeros((int(LEAD_S * sr), channels), dtype=np.float32), x])
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    return y[int(LEAD_S * sr):], rep


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("decay", [0.1, 0.2, 0.5, 2.0, 8.0, 20.0])
@pytest.mark.parametrize("algorithm", ALGORITHMS)
def test_reverb_rt60(sr, decay, algorithm, record_property):
    if decay == 20.0 and sr not in (48000,):
        pytest.skip("20 s measured at 48 k only (render length)")
    y, _ = impulse_response(sr, dict(BASE, algorithm=ALGORITHMS.index(algorithm), decay_s=decay), min(decay * 1.6 + 0.5, 34.0))
    t60 = schroeder_t60(y[:, 0] + y[:, 1], sr)
    record_property("t60_measured", float(t60))
    if algorithm == "Plate" and decay < 0.5:
        # the plate's tank cannot decay faster than ~0.5 s (ADR-0008, reviewer question): labels below reach the floor
        assert abs(t60 - 0.5) / 0.5 < 0.10
    else:
        assert abs(t60 - decay) / decay < 0.10


def test_reverb_predelay():
    sr = 48000
    for pre in (0.0, 50.0, 250.0):
        y, _ = impulse_response(sr, dict(BASE, predelay_ms=pre, early_late_pct=100.0), 1.0)
        first = int(np.argmax(np.abs(y[:, 0]) > 1e-4)) / sr * 1000  # first early reflection
        y0, _ = impulse_response(sr, dict(BASE, predelay_ms=0.0, early_late_pct=100.0), 1.0)
        first0 = int(np.argmax(np.abs(y0[:, 0]) > 1e-4)) / sr * 1000
        assert abs((first - first0) - pre) < 1.0


def window_energy_db(m, sr, win_s=1.0):
    w = int(win_s * sr)
    return [float(10 * np.log10(float(np.sum(m[i:i + w] ** 2)) + 1e-30)) for i in range(w, len(m) - w, w)]


@pytest.mark.parametrize("algorithm", ["Hall", "Plate"])
def test_reverb_stability_long_decay(algorithm, record_property):
    # Review Focus 3: decay 20 s, size 100, diffusion 100, with size and damping moved live during the tail: energy
    # per 1 s window keeps decreasing for 20 s, and no window drops more than 12 dB below the previous one (a size
    # move must not restart or cut the tail)
    sr = 48000
    x = signals.impulse(sr, 22.0, channels=2)
    auto = [{"param": "size_pct", "t_s": 3.0, "value": 40.0}, {"param": "size_pct", "t_s": 3.3, "value": 80.0},
            {"param": "damping_pct", "t_s": 6.0, "value": 60.0}, {"param": "size_pct", "t_s": 9.0, "value": 100.0}]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, algorithm=ALGORITHMS.index(algorithm), decay_s=20.0, size_pct=100.0, diffusion_pct=100.0),
                           input=x, automation=auto)
    assert rep["nan_count"] == 0
    e = window_energy_db(y[:, 0] + y[:, 1], sr)
    record_property("window_energy_db", [round(float(v), 1) for v in e])  # plain floats: xdist serialises them
    assert all(b < a for a, b in zip(e, e[1:]))
    assert all(a - b < 12.0 for a, b in zip(e, e[1:]))


def test_reverb_size_and_algorithm_moves_keep_the_tail():
    # size dragged 60 -> 100 over 0.5 s and rapid algorithm changes (Hall -> Plate -> Room within 200 ms) during a
    # 4 s tail: finite, and no 100 ms window drops more than 12 dB below the previous one
    sr = 48000
    burst = signals.noise(sr, 0.5, seed=12, amp_db=-12.0, channels=2)
    tail_input = np.concatenate([burst, np.zeros((int(3.5 * sr), 2), dtype=np.float32)])
    live_input = signals.noise(sr, 4.0, seed=12, amp_db=-12.0, channels=2)
    drag = [{"param": "size_pct", "t_s": 1.0 + 0.01 * k, "value": 60.0 + 40.0 * k / 50} for k in range(51)]
    rapid = [{"param": "algorithm", "t_s": 1.0, "value": 2}, {"param": "algorithm", "t_s": 1.1, "value": 0}, {"param": "algorithm", "t_s": 1.2, "value": 3}]
    # size moves must keep a free-running tail; algorithm switches morph to an engine that only the live input can
    # fill, so they are judged with the input playing
    for label, auto, params, x in (("size drag Hall", drag, dict(BASE, decay_s=4.0), tail_input),
                                   ("size drag Plate", drag, dict(BASE, algorithm=2, decay_s=4.0), tail_input),
                                   ("rapid algorithm", rapid, dict(BASE, decay_s=4.0), live_input)):
        y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x, automation=auto)
        assert rep["nan_count"] == 0, label
        e = window_energy_db(y[:, 0] + y[:, 1], sr, win_s=0.1)
        drops = [a - b for a, b in zip(e[7:], e[8:])]  # after 0.8 s
        assert max(drops) < 12.0, f"{label}: level dropped {max(drops):.1f} dB in 100 ms"


def test_reverb_predelay_change_no_click():
    sr = 48000
    x = signals.sine(440.0, sr, 2.0, amp_db=-12.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, early_late_pct=100.0, predelay_ms=20.0), input=x,
                           automation=[{"param": "predelay_ms", "t_s": 0.8, "value": 120.0}, {"param": "predelay_ms", "t_s": 1.4, "value": 0.0}])
    assert rep["nan_count"] == 0
    assert np.max(np.abs(np.diff(y[:, 0]))) < 2.0 * np.max(np.abs(np.diff(x[:, 0])))


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
    # Review Focus 3: switching Hall -> Plate mid-tail (input stopped at 0.8 s) morphs rather than clicks: the 10 ms
    # envelope steps no more than the undisturbed tail's own (a reverb tail on noise fluctuates by a few dB per
    # window) plus 2 dB, judged while the level is within 12 dB of the level at the switch (the outgoing engine
    # morphs out over 200 ms with an equal-power law, which steepens by construction below that; the incoming engine
    # starts empty)
    sr = 48000
    x = signals.noise(sr, 0.8, seed=10, amp_db=-12.0, channels=1)
    x = np.concatenate([x, np.zeros((int(1.2 * sr), 1), dtype=np.float32)])

    def max_step(auto):
        y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, decay_s=3.0), input=x, automation=auto)
        assert rep["nan_count"] == 0
        m = y[:, 0]
        w = int(0.01 * sr)
        env = np.array([20 * np.log10(np.sqrt(np.mean(m[i:i + w] ** 2)) + 1e-12) for i in range(int(0.9 * sr), len(m) - w, w)])
        ref = env[int(0.1 / 0.01)]
        audible = (env[1:] > ref - 12.0) & (env[:-1] > ref - 12.0)
        return float(np.max(np.abs(np.diff(env))[audible]))

    switched = max_step([{"param": "algorithm", "t_s": 1.0, "value": 2}])
    undisturbed = max_step([])
    record_property("switch_envelope_step_db", switched)
    record_property("undisturbed_envelope_step_db", undisturbed)
    assert switched < undisturbed + 2.0


def test_reverb_null_mix_zero():
    sr = 48000
    x = signals.noise(sr, 1.0, seed=11, channels=2)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mix_pct=0.0), input=x)
    assert rep["latency_samples"] == 0
    assert measure.residual_dbfs(y, x) < -120.0


def test_reverb_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=12.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    for algo in ALGORITHMS:  # a 0.3 s tail has settled 0.8 s after the switch: residual must be the re-prepared state only
        record_property(f"sr_change_residual_dbfs_{algo}", base_checks.check_sr_change_mid_stream(PLUGIN, params=dict(BASE, algorithm=ALGORITHMS.index(algo), decay_s=0.3), tol_dbfs=-75.0, settle_s=0.8))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=dict(BASE, decay_s=0.2))
    base_checks.check_channel_configs(PLUGIN, params=dict(BASE, early_late_pct=100.0, predelay_ms=0.0, width_pct=200.0), seconds=0.5)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
