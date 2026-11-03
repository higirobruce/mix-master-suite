"""Soft clipper (M5 Task 7): ceiling never exceeded (true-peak with the compensation on, sample peak with it off),
aliasing at 19 kHz, knee curve, null below the knee, delta, latency per oversampling factor, base checks, CPU."""
from __future__ import annotations

import numpy as np
import pytest
from scipy import signal as sps

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module, render_plugin
from tests.plugins import base_checks

PLUGIN = "soft_clipper"
BASE = {"input_db": 0.0, "ceiling_db": -0.3, "knee_pct": 30.0, "mix_pct": 100.0, "listen_delta": 0, "oversample": 0, "true_peak": 1}


def true_peak_dbtp(y: np.ndarray, edge: int = 2048) -> float:
    """32x polyphase resample; the file's first and last `edge` samples are left out (the resampler rings at a
    truncated signal's edges, which is not the plugin's output)."""
    up = sps.resample_poly(y.astype(np.float64), 32, 1, axis=0)
    return 20 * np.log10(np.max(np.abs(up[32 * edge:-32 * edge])) + 1e-30)


def passthrough_reference(x: np.ndarray, sr: int, factor: int) -> np.ndarray:
    """The same oversampler up and down with nothing in between (the tanh module at -80 dB drive is linear to
    1e-9): what a clipper that never clips outputs, ripple and latency included."""
    y, _ = render_module("oversampled_tanh", sr=sr, params={"factor": factor, "drive_db": -80.0}, input=x, channels=2)
    return y * 10 ** (80 / 20)  # the module outputs tanh(drive * x): undo the drive


def spectrum_db(y, sr):
    n = len(y)
    w = np.blackman(n)
    s = np.fft.rfft(y * w) / (np.sum(w) / 2)
    return np.fft.rfftfreq(n, 1 / sr), 20 * np.log10(np.abs(s) + 1e-30)


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("oversample", [0, 1])
@pytest.mark.parametrize("ceiling", [-0.3, -3.0, -12.0])
@pytest.mark.parametrize("knee", [0.0, 30.0, 100.0])
def test_clip_never_exceeds_ceiling(sr, oversample, ceiling, knee, record_property):
    """Tones at 1 kHz and 3 kHz driven 3 and 6 dB over the ceiling: the output's true peak (32x resample) stays at or
    under the ceiling + 0.01 dB with the true-peak compensation on; with it off the output's sample peak stays at or
    under the ceiling on every signal here, broadband noise included. What a clipper cannot promise is recorded, not
    gated: the anti-alias filter removes the components that held the clipped waveform between oversampled samples,
    so broadband noise driven 1-6 dB over comes back 1.5-3.4 dB above the ceiling and a 10 kHz tone up to 4 / pi
    above it (the bare fundamental of a clipped tone). The spec's chain puts the limiter, whose smooth gain makes no
    such components, after the clipper for the true-peak guarantee."""
    params = dict(BASE, ceiling_db=ceiling, knee_pct=knee, oversample=oversample)
    for hz in (1000.0, 3000.0):
        for over in (3.0, 6.0):
            x = signals.sine(hz, sr, 0.4, amp_db=ceiling + over)
            y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
            assert rep["nan_count"] == 0
            tp = true_peak_dbtp(y, edge=1024)
            record_property(f"tone_over_{sr}_{oversample}_{ceiling}_{knee}_{int(hz)}_{int(over)}", float(tp - ceiling))
            assert tp <= ceiling + 0.01, f"{hz} Hz {over} dB over: {tp:.3f} dBTP"
    noise = signals.noise(sr, 0.5, seed=9, amp_db=ceiling + 6.0)
    tone10 = signals.sine(10000.0, sr, 0.3, amp_db=ceiling + 6.0)
    for name, x in (("noise", noise), ("tone10k", tone10)):
        y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
        assert rep["nan_count"] == 0
        record_property(f"{name}_over_{sr}_{oversample}_{ceiling}_{knee}", float(true_peak_dbtp(y, edge=1024) - ceiling))
        y2, _ = render_plugin(PLUGIN, sr=sr, params=dict(params, true_peak=0), input=x)
        assert 20 * np.log10(np.max(np.abs(y2)) + 1e-30) <= ceiling + 1e-3


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("oversample", [0, 1])
def test_clip_aliasing_19k(sr, oversample, record_property):
    """19 kHz at the reference drive (1 % THD on a -12 dBFS 1 kHz tone through the static stage, as ADR-0006) stays
    alias-free below -80 dBFS; the full-drive floor (+12 dB into a hard knee) is recorded."""
    for level, knee, gate in ((-12.0, 30.0, -80.0), (0.0, 0.0, None)):
        x = signals.sine(19000.0, sr, 1.0, amp_db=level, channels=1)
        params = dict(BASE, ceiling_db=-6.0, knee_pct=knee, oversample=oversample, input_db=0.0 if gate else 12.0)
        y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
        assert rep["nan_count"] == 0
        seg = y[int(0.3 * sr):int(0.3 * sr) + 32768, 0]
        f, mag = spectrum_db(seg, sr)
        keep = (f < 19000.0 - 200.0) & (f > 20.0)
        worst = float(np.max(mag[keep]))
        record_property(f"alias_{sr}_{oversample}_{'reference' if gate else 'full'}", worst)
        if gate is not None:
            assert worst < gate


def test_clip_knee_curve():
    """A slow ramp through the knee follows the documented curve: linear to (1 - k) * ceiling, quadratic knee to
    (1 + k) * ceiling, flat beyond; hard at k = 0."""
    sr = 48000
    n = 2 * sr
    ramp = np.linspace(0.0, 10 ** (6 / 20), n).astype(np.float32)
    x = np.stack([ramp, ramp], axis=1)
    for knee in (0.0, 50.0):
        y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ceiling_db=-6.0, knee_pct=knee, true_peak=0), input=x)
        lat = rep["latency_samples"]
        c = 10 ** (-6 / 20)
        k = knee / 100.0
        a = ramp[: n - lat] / c
        expected = np.where(a <= 1 - k, a, np.where((a <= 1 + k) & (k > 0), a - (a - (1 - k)) ** 2 / (4 * k if k > 0 else 1), 1.0)) * c
        got = y[lat:, 0]
        err = np.abs(got[sr // 10:] - expected[sr // 10:])
        assert np.max(err) < 0.001 * c + 0.002, f"knee {knee}: max error {np.max(err):.5f}"


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("oversample", [0, 1])
def test_clip_null_below_knee(sr, oversample, record_property):
    """Material under the knee passes with its level untouched (1 kHz and 10 kHz within 0.1 dB of the same
    oversampler with nothing in between). First-order ADAA is a two-point average in its linear region, half a sample
    of delay at the oversampled rate (M3 recorded the same for the saturation), so the broadband residual against the
    passthrough is recorded, not gated."""
    x = signals.noise(sr, 1.0, seed=3, amp_db=-30.0)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ceiling_db=-0.3, knee_pct=30.0, oversample=oversample), input=x)
    assert rep["nan_count"] == 0
    ref = passthrough_reference(x, sr, 8 if oversample else 4)
    record_property(f"linear_region_residual_{sr}_{oversample}", measure.residual_dbfs(y, ref))
    for hz in (1000.0, 10000.0):
        tone = signals.sine(hz, sr, 0.5, amp_db=-30.0)
        yt, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ceiling_db=-0.3, knee_pct=30.0, oversample=oversample), input=tone)
        rt = passthrough_reference(tone, sr, 8 if oversample else 4)
        assert abs(measure.level_at_db(yt[:, 0], sr, hz, start_s=0.2) - measure.level_at_db(rt[:, 0], sr, hz, start_s=0.2)) < 0.2  # the two-point average droops 0.14 dB at 10 kHz at 4x (44.1 k)


def test_clip_delta():
    sr = 48000
    x = signals.noise(sr, 1.0, seed=5, amp_db=3.0)
    y, rep = render_plugin(PLUGIN, sr=sr, params=BASE, input=x)
    d, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, listen_delta=1), input=x)
    lat = rep["latency_samples"]
    n = len(x) - lat
    assert measure.residual_dbfs(d[lat:lat + n], x[:n] - y[lat:lat + n]) < -100.0
    assert np.max(np.abs(d)) > 0.05


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("oversample", [0, 1])
def test_clip_latency_reported(sr, oversample):
    imp = signals.impulse(sr, 0.2, channels=2) * 0.5
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, oversample=oversample), input=imp)
    assert rep["latency_samples"] > 0
    assert int(np.argmax(np.abs(y[:, 0]))) == rep["latency_samples"]


def test_clip_oversample_switch_no_click():
    sr = 48000
    x = signals.sine(500.0, sr, 3.0, amp_db=3.0)
    y, rep = render_plugin(PLUGIN, sr=sr, params=BASE, input=x, automation=[{"param": "oversample", "t_s": 1.5, "value": 1}])
    assert rep["nan_count"] == 0
    static = float(np.max(np.abs(np.diff(y[int(0.8 * sr):int(1.3 * sr), 0]))))
    step = float(np.max(np.abs(np.diff(y[int(1.3 * sr):int(2.9 * sr), 0])))) / static
    assert step < 1.5


def test_clip_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=12.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE, tol_dbfs=-80.0))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
