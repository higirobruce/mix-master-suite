"""M/S processor (M5 Task 6): encode / decode null, mid and side gains, width (imager semantics), per-channel EQ
against the matched-biquad reference, mono-below, solo, click-free width sweep, base checks, CPU."""
from __future__ import annotations

import numpy as np
import pytest

from tests.harness import designs, measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "ms_processor"
BASE = {"mid_db": 0.0, "side_db": 0.0, "width_pct": 100.0, "mono_below_hz": 0.0, "solo": 0, "output_db": 0.0}
TYPES = {"Bell": 0, "Low shelf": 1, "High shelf": 2}


def stereo_noise(sr, seconds=1.0, seed=31):
    l = signals.noise(sr, seconds, seed=seed, amp_db=-18.0, channels=1)
    r = signals.noise(sr, seconds, seed=seed + 100, amp_db=-18.0, channels=1)
    return np.concatenate([l, r], axis=1).astype(np.float32)


def mono_tone(sr, hz=1000.0, db=-12.0):
    return signals.sine(hz, sr, 1.0, amp_db=db, channels=2)


def side_tone(sr, hz=1000.0, db=-12.0):
    x = signals.sine(hz, sr, 1.0, amp_db=db, channels=2)
    x[:, 1] *= -1.0
    return x


def level(y, sr, hz):
    return measure.level_at_db(y[:, 0], sr, hz, start_s=0.5)


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_ms_encode_decode_null(sr, record_property):
    x = stereo_noise(sr)
    y, rep = render_plugin(PLUGIN, sr=sr, params=BASE, input=x)
    assert rep["nan_count"] == 0 and rep["latency_samples"] == 0
    res = measure.residual_dbfs(y, x)
    record_property(f"null_{sr}", res)
    assert res < -120.0


def test_ms_gains():
    sr = 48000
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mid_db=6.0), input=mono_tone(sr))
    assert abs(level(y, sr, 1000.0) + 12.0 - 6.0) < 0.02
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, side_db=6.0), input=side_tone(sr))
    assert abs(level(y, sr, 1000.0) + 12.0 - 6.0) < 0.02
    # the other channel is untouched
    x = side_tone(sr)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mid_db=6.0), input=x)
    assert measure.residual_dbfs(y, x) < -100.0
    x = mono_tone(sr)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, side_db=6.0), input=x)
    assert measure.residual_dbfs(y, x) < -100.0


def test_ms_width():
    sr = 48000
    x = stereo_noise(sr)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_pct=0.0), input=x)
    assert abs(float(np.corrcoef(y[:, 0], y[:, 1])[0, 1]) - 1.0) < 0.001
    ys, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_pct=200.0), input=side_tone(sr))
    assert abs(level(ys, sr, 1000.0) + 12.0 - 6.02) < 0.1
    for width in (0.0, 50.0, 150.0, 200.0):
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_pct=width), input=x)
        assert measure.residual_dbfs(y[:, 0] + y[:, 1], x[:, 0] + x[:, 1]) < -100.0


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("channel", ["mid", "side"])
@pytest.mark.parametrize("type_name,hz,gain,q", [("Bell", 1000.0, 6.0, 1.0), ("Low shelf", 200.0, -4.0, 0.7), ("High shelf", 6000.0, 3.0, 0.7)])
def test_ms_eq_response(sr, channel, type_name, hz, gain, q):
    """Each EQ type on mid or side matches the matched-biquad reference within 0.1 dB / 2 deg on an impulse of that
    channel; the other channel is untouched."""
    kind = {"Bell": "peak", "Low shelf": "lowshelf", "High shelf": "highshelf"}[type_name]
    params = dict(BASE, **{f"{channel}_eq1_enabled": 1, f"{channel}_eq1_type": TYPES[type_name], f"{channel}_eq1_freq_hz": hz,
                           f"{channel}_eq1_gain_db": gain, f"{channel}_eq1_q": q})
    n = 65536
    imp = signals.impulse(sr, n / sr, channels=2)
    if channel == "side":
        imp[:, 1] *= -1.0
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=imp)
    assert rep["nan_count"] == 0
    ir = y[:n, 0].astype(np.float64)
    f = np.geomspace(20.0, 0.45 * sr, 100)
    nn = np.arange(n)
    h = np.array([np.sum(ir * np.exp(-2j * np.pi * fi * nn / sr)) for fi in f]) / imp[0, 0]
    b, a = designs.matched(kind, sr, hz, gain, q)
    ref_db, ref_deg = designs.freqz_db_deg(b, a, f, sr)
    assert np.max(np.abs(20 * np.log10(np.abs(h)) - ref_db)) < 0.1
    phase_err = (np.degrees(np.angle(h)) - ref_deg + 180.0) % 360.0 - 180.0
    assert np.max(np.abs(phase_err)) < 2.0
    other = mono_tone(sr) if channel == "side" else side_tone(sr)
    yo, _ = render_plugin(PLUGIN, sr=sr, params=params, input=other)
    assert measure.residual_dbfs(yo, other) < -100.0


def test_ms_mono_below(record_property):
    sr = 48000
    for f, mono, limit in ((40.0, 300.0, -60.0), (60.0, 200.0, -36.0)):
        x = side_tone(sr, f, -20.0)
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mono_below_hz=mono), input=x)
        att = measure.level_at_db(y[:, 0], sr, f, start_s=0.3) - measure.level_at_db(x[:, 0], sr, f, start_s=0.3)
        record_property(f"side_attenuation_{int(f)}hz_mono_{int(mono)}", float(att))
        assert att < limit
    xh = side_tone(sr, 2000.0, -20.0)
    yh, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mono_below_hz=200.0), input=xh)
    assert abs(measure.level_at_db(yh[:, 0], sr, 2000.0, start_s=0.3) - measure.level_at_db(xh[:, 0], sr, 2000.0, start_s=0.3)) < 0.05


def test_ms_solo():
    sr = 48000
    x = mono_tone(sr, 500.0) + side_tone(sr, 3000.0)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, solo=1), input=x)
    assert level(y, sr, 3000.0) < -80.0 and abs(level(y, sr, 500.0) + 12.0) < 0.05
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, solo=2), input=x)
    assert level(y, sr, 500.0) < -80.0 and abs(level(y, sr, 3000.0) + 12.0) < 0.05


def test_ms_width_sweep_no_click():
    # width automated 0 -> 200 while a stereo tone pair runs: no sample jump above 1.1x the width-200 output's own
    # slope (as the imager's test)
    sr = 48000
    x = signals.sine(440.0, sr, 2.0, amp_db=-12.0, channels=2)
    x[:, 1] = signals.sine(660.0, sr, 2.0, amp_db=-12.0, channels=1)[:, 0]
    auto = [{"param": "width_pct", "t_s": 0.5 + 0.01 * k, "value": 200.0 * k / 100} for k in range(101)]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_pct=0.0), input=x, automation=auto)
    assert rep["nan_count"] == 0
    wide, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_pct=200.0), input=x)
    assert np.max(np.abs(np.diff(y[:, 0]))) < 1.1 * np.max(np.abs(np.diff(wide[:, 0])))
    auto2 = [{"param": "mono_below_hz", "t_s": 0.5, "value": 200.0}, {"param": "mono_below_hz", "t_s": 0.9, "value": 400.0},
             {"param": "mono_below_hz", "t_s": 1.2, "value": 0.0}]
    xs = side_tone(sr, 80.0)
    xs = np.concatenate([xs, xs], axis=0)
    y2, rep2 = render_plugin(PLUGIN, sr=sr, params=BASE, input=xs, automation=auto2)
    assert rep2["nan_count"] == 0
    assert np.max(np.abs(np.diff(y2[:, 0]))) < 2.0 * np.max(np.abs(np.diff(xs[:, 0])))


def test_ms_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=40.0)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
