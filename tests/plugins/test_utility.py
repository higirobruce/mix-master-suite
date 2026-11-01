import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.render import SAMPLE_RATES, render_plugin
from tests.plugins import base_checks

PLUGIN = "utility"


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_null_flat(sr, record_property):
    x = signals.noise(sr, 2.0, seed=1)
    y, _ = render_plugin(PLUGIN, sr=sr, input=x)
    res = measure.residual_dbfs(y, x)
    record_property("null_residual_dbfs", res)
    assert res < -120


@pytest.mark.parametrize("sr", [48000])
@pytest.mark.parametrize("gain", [-24, -6, 0, 6, 24])
def test_gain_accuracy(sr, gain, record_property):
    x = signals.sine(1000, sr, 1.0, amp_db=-30)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"gain_db": gain}, input=x)
    err = (measure.level_at_db(y[:, 0], sr, 1000, start_s=0.5) - measure.level_at_db(x[:, 0], sr, 1000, start_s=0.5)) - gain
    record_property("gain_error_db", float(err))
    assert abs(err) < 0.01


def test_polarity_flip():
    sr = 48000
    x = signals.sine(1000, sr, 0.1, amp_db=-6)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"polarity_l": 1}, input=x)
    np.testing.assert_allclose(y[:, 0], -x[:, 0], atol=1e-6)
    np.testing.assert_allclose(y[:, 1], x[:, 1], atol=1e-6)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"polarity_r": 1}, input=x)
    np.testing.assert_allclose(y[:, 1], -x[:, 1], atol=1e-6)


def test_swap_lr():
    sr = 48000
    x = signals.noise(sr, 0.1, seed=2)
    x[:, 1] *= 0.5
    y, _ = render_plugin(PLUGIN, sr=sr, params={"swap_lr": 1}, input=x)
    np.testing.assert_allclose(y[:, 0], x[:, 1], atol=1e-6)
    np.testing.assert_allclose(y[:, 1], x[:, 0], atol=1e-6)


def test_mute_l_r():
    sr = 48000
    x = signals.noise(sr, 0.1, seed=3)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"mute_l": 1}, input=x)
    assert np.max(np.abs(y[:, 0])) == 0.0 and np.max(np.abs(y[:, 1])) > 0.1
    y, _ = render_plugin(PLUGIN, sr=sr, params={"mute_r": 1}, input=x)
    assert np.max(np.abs(y[:, 1])) == 0.0 and np.max(np.abs(y[:, 0])) > 0.1


def test_mono_sum():
    sr = 48000
    x = signals.noise(sr, 0.1, seed=4)
    x[:, 1] = signals.noise(sr, 0.1, seed=5)[:, 0]
    y, _ = render_plugin(PLUGIN, sr=sr, params={"mono_sum": 1}, input=x)
    m = 0.5 * (x[:, 0] + x[:, 1])
    np.testing.assert_allclose(y[:, 0], m, atol=1e-6)
    np.testing.assert_allclose(y[:, 1], m, atol=1e-6)


def test_dc_block(record_property):
    sr = 48000
    y, _ = render_plugin(PLUGIN, sr=sr, params={"dc_block": 1}, input=signals.dc(sr, 2.0))
    dc_db = measure.rms_db(y[int(1.5 * sr):, 0])
    record_property("dc_block_rejection_db", float(dc_db))
    assert dc_db < -60
    x = signals.sine(1000, sr, 1.0, amp_db=-12)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"dc_block": 1}, input=x)
    assert abs(measure.level_at_db(y[:, 0], sr, 1000, start_s=0.5) - measure.level_at_db(x[:, 0], sr, 1000, start_s=0.5)) < 0.05
    y, _ = render_plugin(PLUGIN, sr=sr, params={"dc_block": 0}, input=signals.dc(sr, 0.5))
    assert np.allclose(y[:, 0], 1.0)


@pytest.mark.parametrize("deg", [45, 90, 135])
def test_phase_rotation(deg, record_property):
    sr = 48000
    x = signals.sine(100, sr, 2.0, amp_db=-12)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"phase_rotation_deg": deg}, input=x)
    ph = measure.phase_at_deg(y[:, 0], x[:, 0], sr, 100, start_s=1.0)
    mag = measure.level_at_db(y[:, 0], sr, 100, start_s=1.0) - measure.level_at_db(x[:, 0], sr, 100, start_s=1.0)
    record_property("phase_error_deg", float(ph + deg))
    assert abs(ph + deg) < 1.0
    assert abs(mag) < 0.01


def test_phase_rotation_zero_is_exact():
    sr = 48000
    x = signals.noise(sr, 0.5, seed=6)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"phase_rotation_deg": 0}, input=x)
    assert measure.residual_dbfs(y, x) < -120


def test_trim_and_gain_add():
    sr = 48000
    x = signals.noise(sr, 0.5, seed=7)
    y, _ = render_plugin(PLUGIN, sr=sr, params={"gain_db": 6, "trim_db": -6}, input=x)
    assert measure.residual_dbfs(y, x) < -120


@pytest.mark.parametrize("host_bypass", [False, True])
def test_bypass_gain_match(host_bypass):
    sr = 48000
    x = signals.sine(1000, sr, 2.0, amp_db=-24)
    job = {"params": {"gain_db": 12}}
    if host_bypass:
        job["bypass_host_at_s"] = 1.0
    else:
        job["automation"] = [{"param": "bypass", "t_s": 1.0, "value": 1}]
    y, _ = render_plugin(PLUGIN, sr=sr, input=x, **job)
    wet = measure.rms_db(y[int(0.5 * sr):int(0.9 * sr), 0])
    byp = measure.rms_db(y[int(1.3 * sr):int(1.5 * sr), 0])
    assert abs(byp - wet) < 0.1


def test_mono_to_stereo():
    sr = 48000
    x = signals.sine(1000, sr, 0.1, amp_db=-12, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params={"gain_db": -6}, input=x, layout={"in": 1, "out": 2})
    assert y.shape[1] == 2
    np.testing.assert_allclose(y[:, 0], y[:, 1], atol=1e-7)
    assert abs(measure.level_at_db(y[:, 0], sr, 1000, start_s=0.05) - (-18)) < 0.05


def test_variable_block_sizes(record_property):
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN))


def test_sr_change_mid_stream(record_property):
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN))


def test_state_roundtrip():
    base_checks.check_state_roundtrip(PLUGIN)


def test_param_extremes():
    base_checks.check_param_extremes(PLUGIN, max_peak_db=24.5)


def test_denormal_tail():
    base_checks.check_denormal_tail(PLUGIN)


def test_channel_configs():
    base_checks.check_channel_configs(PLUGIN)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
