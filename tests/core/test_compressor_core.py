import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module

DEFAULTS = {"threshold_db": -20.0, "ratio": 4.0, "knee_db": 0.0, "attack_ms": 1.0, "release_ms": 100.0,
            "lookahead_ms": 0.0, "sc_hp_hz": 0.0, "expander": 0, "range_db": -80.0, "detector": 0}
RMS = {"detector": 1}  # static-curve measurements use the RMS detector: steady state on a sine is exact there
RMS_OFFSET_DB = -3.0103  # RMS of a sine relative to its peak


def out_level(sr, in_db, **over):
    p = dict(DEFAULTS, **over)
    x = signals.sine(1000, sr, 1.0, amp_db=in_db, channels=1)
    y, rep = render_module("compressor_core", sr=sr, params=p, input=x)
    assert rep["nan_count"] == 0
    return measure.level_at_db(y[:, 0], sr, 1000, start_s=0.6), rep


@pytest.mark.parametrize("sr", [48000])
@pytest.mark.parametrize("ratio", [2.0, 4.0, 8.0, 20.0, 1000.0])
def test_static_ratio_within_0p1(sr, ratio, record_property):
    levels = [-30.0, -12.0, -6.0, 0.0]  # sine peak levels; the RMS detector sees them 3 dB lower (all but the first above -20)
    outs = [out_level(sr, L, ratio=ratio, **RMS)[0] for L in levels]
    # below threshold: unity
    assert abs(outs[0] - levels[0]) < 0.05
    # above threshold: slope = 1/ratio
    for a, b in ((1, 2), (2, 3)):
        slope = (outs[b] - outs[a]) / (levels[b] - levels[a])
        measured_ratio = 1.0 / slope if slope > 1e-6 else 1e9
        record_property(f"ratio_{ratio}_measured", float(measured_ratio))
        if ratio >= 1000:
            assert slope < 0.005
        else:
            assert abs(measured_ratio - ratio) < 0.1 * (1 if ratio <= 8 else ratio / 8)  # 0.1 absolute up to 8:1


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_unity_ratio_nulls(sr):
    x = signals.noise(sr, 1.0, seed=5, channels=1)
    y, _ = render_module("compressor_core", sr=sr, params=dict(DEFAULTS, ratio=1.0), input=x)
    assert measure.residual_dbfs(y, x) < -90


def _gain_envelope_db(x, y, sr, dc):
    if dc:
        return 20 * np.log10(np.abs(y[:, 0]) / (np.abs(x[:, 0]) + 1e-12) + 1e-12)
    w = int(0.004 * sr)
    gx = np.sqrt(np.convolve(x[:, 0] ** 2, np.ones(w) / w, "same"))
    gy = np.sqrt(np.convolve(y[:, 0] ** 2, np.ones(w) / w, "same"))
    return 20 * np.log10(gy / (gx + 1e-12) + 1e-12)


def _burst(sr, release_ms, dc):
    n = int((0.6 + 4 * release_ms / 1000) * sr)
    t = np.arange(n) / sr
    carrier = np.ones(n) if dc else np.sin(2 * np.pi * 1000 * t)
    return (carrier * np.where((t >= 0.1) & (t < 0.4), 1.0, 10 ** (-40 / 20))).astype(np.float32)[:, None]


def _times(g, sr, gr):
    on = g[int(0.1 * sr) + 2:int(0.4 * sr) - 2]
    off = g[int(0.4 * sr) + 2:]
    atk = (np.argmax(on <= -0.9 * gr) - np.argmax(on <= -0.1 * gr)) / sr * 1000
    rel = (np.argmax(off >= -0.1 * gr) - np.argmax(off >= -0.9 * gr)) / sr * 1000
    return atk, rel


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("attack_ms,release_ms", [(1.0, 50.0), (10.0, 300.0), (100.0, 1000.0)])
def test_attack_release_step_within_10_percent(sr, attack_ms, release_ms, record_property):
    """Step test (spec: 'step and tone-burst tests'): a 40 dB DC step with threshold -20, 4:1 gives 15 dB of gain
    reduction; the 10-90 % times of the reduction match the settings within 10 %."""
    x = _burst(sr, release_ms, dc=True)
    y, rep = render_module("compressor_core", sr=sr, params=dict(DEFAULTS, attack_ms=attack_ms, release_ms=release_ms), input=x)
    assert rep["nan_count"] == 0
    g = _gain_envelope_db(x, y, sr, dc=True)
    on = g[int(0.1 * sr) + 2:int(0.4 * sr) - 2]
    gr = -np.mean(on[-len(on) // 5:])
    record_property("gr_db", float(gr))
    assert abs(gr - 15.0) < 0.1
    atk, rel = _times(g, sr, gr)
    record_property("attack_ms_measured", float(atk))
    record_property("release_ms_measured", float(rel))
    assert abs(atk - attack_ms) <= 0.1 * attack_ms + 1000.0 / sr
    assert abs(rel - release_ms) <= 0.1 * release_ms + 1000.0 / sr


@pytest.mark.parametrize("sr", [48000, 192000])
@pytest.mark.parametrize("attack_ms,release_ms", [(1.0, 50.0), (100.0, 1000.0)])
def test_tone_burst_characterisation(sr, attack_ms, release_ms, record_property):
    """Tone burst with the peak detector: the half-cycle ripple of |sin| means an attack longer than a quarter period
    cannot catch every peak, so the settled reduction sits below 15 dB and the apparent attack is longer. Recorded
    for the M2 compressor ADR; bounded loosely here so a regression (e.g. losing the gain-domain ballistics) shows."""
    x = _burst(sr, release_ms, dc=False)
    y, rep = render_module("compressor_core", sr=sr, params=dict(DEFAULTS, attack_ms=attack_ms, release_ms=release_ms), input=x)
    assert rep["nan_count"] == 0
    g = _gain_envelope_db(x, y, sr, dc=False)
    on = g[int(0.1 * sr) + int(0.004 * sr):int(0.4 * sr) - int(0.004 * sr)]
    gr = -np.mean(on[-len(on) // 5:])
    atk, rel = _times(g, sr, gr)
    record_property("gr_db", float(gr))
    record_property("attack_ms_measured", float(atk))
    record_property("release_ms_measured", float(rel))
    assert 12.0 < gr <= 15.1
    assert atk < 2.0 * attack_ms + 4.0 + 8000.0 / sr  # + the 4 ms RMS measurement window
    assert 0.5 * release_ms < rel < 2.0 * release_ms + 8000.0 / sr


def test_lookahead_prevents_overshoot():
    sr = 48000
    n = int(0.5 * sr)
    t = np.arange(n) / sr
    x = (np.sin(2 * np.pi * 1000 * t) * (t >= 0.2)).astype(np.float32)[:, None]
    y, rep = render_module("compressor_core", sr=sr, params=dict(DEFAULTS, attack_ms=0.1, lookahead_ms=5.0, ratio=1000.0),
                           input=x)
    assert rep["latency_samples"] == 240
    peak_db = 20 * np.log10(np.max(np.abs(y[int(0.2 * sr):, 0])) + 1e-12)
    assert peak_db < -20.0 + 0.5  # never above threshold + 0.5 dB with infinite ratio


def test_sidechain_highpass_ignores_lows():
    sr = 48000
    low = signals.sine(50, sr, 1.0, amp_db=-10, channels=1)
    mid = signals.sine(1000, sr, 1.0, amp_db=-10, channels=1)
    y_low, _ = render_module("compressor_core", sr=sr, params=dict(DEFAULTS, sc_hp_hz=200.0, **RMS), input=low)
    y_mid, _ = render_module("compressor_core", sr=sr, params=dict(DEFAULTS, sc_hp_hz=200.0, **RMS), input=mid)
    gr_low = measure.level_at_db(low[:, 0], sr, 50, start_s=0.6) - measure.level_at_db(y_low[:, 0], sr, 50, start_s=0.6)
    gr_mid = measure.level_at_db(mid[:, 0], sr, 1000, start_s=0.6) - measure.level_at_db(y_mid[:, 0], sr, 1000, start_s=0.6)
    expected_mid = 0.75 * ((-10.0 + RMS_OFFSET_DB) + 20.0)  # 4:1 on the RMS level above a -20 dB threshold
    assert gr_low < 0.2
    assert abs(gr_mid - expected_mid) < 0.3
