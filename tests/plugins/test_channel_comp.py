import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "channel_comp"
RMS_OFFSET_DB = -3.0103
BASE = {"threshold_db": -20.0, "ratio": 4.0, "knee_db": 0.0, "attack_ms": 1.0, "release_ms": 100.0, "detector": 1,
        "mix_pct": 100.0, "makeup_db": 0.0, "stereo_link_pct": 100.0}


def out_level_db(sr, in_db, **over):
    x = signals.sine(1000, sr, 1.0, amp_db=in_db, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, **over), input=x)
    assert rep["nan_count"] == 0
    return measure.level_at_db(y[:, 0], sr, 1000, start_s=0.6)


@pytest.mark.parametrize("ratio", [2.0, 4.0, 8.0, 20.0])
def test_comp_ratio_within_0p1(ratio, record_property):
    levels = [-30.0, -12.0, -6.0, 0.0]  # peak levels; the RMS detector sees 3 dB less; all but the first above -20
    outs = [out_level_db(48000, L, ratio=ratio) for L in levels]
    assert abs(outs[0] - levels[0]) < 0.05
    for a, b in ((1, 2), (2, 3)):
        slope = (outs[b] - outs[a]) / (levels[b] - levels[a])
        record_property(f"ratio_{ratio}_measured", float(1 / slope))
        assert abs(1 / slope - ratio) < 0.1


def test_comp_infinite_ratio():
    outs = [out_level_db(48000, L, ratio_inf=1) for L in (-6.0, 0.0)]
    assert abs(outs[1] - outs[0]) < 0.05


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("attack_ms,release_ms", [(1.0, 50.0), (10.0, 300.0), (100.0, 1000.0)])
def test_comp_attack_release_step_within_10_percent(sr, attack_ms, release_ms, record_property):
    n = int((0.6 + 4 * release_ms / 1000) * sr)
    t = np.arange(n) / sr
    x = np.where((t >= 0.1) & (t < 0.4), 1.0, 10 ** (-40 / 20)).astype(np.float32)[:, None]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, detector=0, attack_ms=attack_ms, release_ms=release_ms), input=x)
    assert rep["nan_count"] == 0
    g = 20 * np.log10(np.abs(y[:, 0]) / (np.abs(x[:, 0]) + 1e-12) + 1e-12)
    on = g[int(0.1 * sr) + 2:int(0.4 * sr) - 2]
    off = g[int(0.4 * sr) + 2:]
    gr = -np.mean(on[-len(on) // 5:])
    assert abs(gr - 15.0) < 0.1
    atk = (np.argmax(on <= -0.9 * gr) - np.argmax(on <= -0.1 * gr)) / sr * 1000
    rel = (np.argmax(off >= -0.1 * gr) - np.argmax(off >= -0.9 * gr)) / sr * 1000
    record_property("attack_ms_measured", float(atk))
    record_property("release_ms_measured", float(rel))
    assert abs(atk - attack_ms) <= 0.1 * attack_ms + 1000.0 / sr
    assert abs(rel - release_ms) <= 0.1 * release_ms + 1000.0 / sr


def test_comp_knee_curve():
    # knee 12 dB, 4:1, threshold -20: at the threshold the reduction is (1/4-1)(6)^2/(2*12) = -1.125 dB
    sr = 48000
    a = out_level_db(sr, -20.0 - RMS_OFFSET_DB, knee_db=12.0)  # RMS level exactly at threshold
    expected = -20.0 - RMS_OFFSET_DB - 1.125
    assert abs(a - expected) < 0.1
    b = out_level_db(sr, -28.0 - RMS_OFFSET_DB, knee_db=12.0)  # below the knee: untouched
    assert abs(b - (-28.0 - RMS_OFFSET_DB)) < 0.05


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_comp_null_unity(sr):
    x = signals.noise(sr, 1.0, seed=41)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ratio=1.0, knee_db=0.0), input=x)
    assert measure.residual_dbfs(y, x) < -90


def test_comp_feedback_topology_compresses_less(record_property):
    ff = out_level_db(48000, 0.0, topology=0)
    fb = out_level_db(48000, 0.0, topology=1)
    record_property("ff_out_db", float(ff))
    record_property("fb_out_db", float(fb))
    assert fb > ff  # feedback detector sees the compressed signal: less reduction at the same ratio
    assert fb < 0.0 - 1.0  # but still compressing


def test_comp_sidechain_external_key():
    sr = 48000
    main = signals.sine(1000, sr, 1.0, amp_db=0.0, channels=1)
    # sidechain fed with silence (the harness feeds sidechain channels from the input: use a job with a quiet input
    # and loud sidechain by swapping roles) -> ext key on with silence must not compress the loud main
    y_loud_main, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ext_key=1), input=main, layout={"in": 1, "out": 1, "sidechain": 1},
                                   sidechain_db=-80.0)
    assert abs(measure.level_at_db(y_loud_main[:, 0], sr, 1000, start_s=0.6) - 0.0) < 0.1


def test_comp_ext_key_without_bus_falls_back_to_internal():
    sr = 48000
    x = signals.sine(1000, sr, 1.0, amp_db=0.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ext_key=1), input=x, layout={"in": 1, "out": 1, "sidechain": 0})
    assert rep["nan_count"] == 0
    assert measure.level_at_db(y[:, 0], sr, 1000, start_s=0.6) < -10.0  # compressed by the internal key


def test_comp_mono_link_is_noop():
    sr = 48000
    x = signals.sine(1000, sr, 1.0, amp_db=0.0, channels=1)
    a, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, stereo_link_pct=100.0), input=x, layout={"in": 1, "out": 1})
    b, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, stereo_link_pct=0.0), input=x, layout={"in": 1, "out": 1})
    assert measure.residual_dbfs(a, b) < -120


def test_comp_stereo_link_blend():
    sr = 48000
    x = signals.sine(1000, sr, 1.0, amp_db=0.0, channels=2)
    x[:, 1] *= 10 ** (-30 / 20)  # right channel 30 dB down
    linked, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, stereo_link_pct=100.0), input=x)
    unlinked, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, stereo_link_pct=0.0), input=x)
    r_linked = measure.level_at_db(linked[:, 1], sr, 1000, start_s=0.6)
    r_unlinked = measure.level_at_db(unlinked[:, 1], sr, 1000, start_s=0.6)
    assert r_linked < r_unlinked - 10.0  # linked: the quiet right channel follows the loud left's reduction


def test_comp_mix_blend():
    sr = 48000
    x = signals.sine(1000, sr, 1.0, amp_db=0.0, channels=1)
    wet, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mix_pct=100.0), input=x)
    half, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mix_pct=50.0), input=x)
    seg = slice(int(0.6 * sr), None)
    np.testing.assert_allclose(half[seg, 0], 0.5 * wet[seg, 0] + 0.5 * x[seg, 0], atol=2e-4)


def test_comp_auto_makeup():
    sr = 48000
    quiet = out_level_db(sr, -30.0, auto_makeup=1)
    assert abs(quiet - (-30.0 + 20.0 * 0.75 / 2)) < 0.1  # +|T|(1-1/R)/2 = +7.5 dB on an uncompressed signal


def test_comp_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=30.0, ramp_params={"makeup_db": 0.1})
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE, tol_dbfs=-60.0))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
