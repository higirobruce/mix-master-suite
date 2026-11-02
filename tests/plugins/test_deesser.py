import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "deesser"
MODES = ["Band", "Split"]
# width 0.7 oct (Q 2.0): a -18 dB bell at 6 kHz then sits within 0.03 dB at 1 kHz (ADR-0009 isolation gate)
BASE = {"freq_hz": 6000.0, "threshold_db": -24.0, "range_db": 24.0, "mode": 0, "width_oct": 0.7, "listen": 0, "lookahead_ms": 0.0}


def for_mode(mode, **over):
    """Band: bell centred on the 6 kHz sibilance. Split: crossover at 4 kHz so the sibilance sits in the high band
    (a tone exactly at an LR4 crossover is half in each band by design)."""
    p = dict(BASE, mode=MODES.index(mode), **over)
    if mode == "Split":
        p["freq_hz"] = 4000.0
    return p


def two_tones(sr, dur=1.0, low_db=-12.0, high_db=-6.0):
    return (signals.sine(1000.0, sr, dur, amp_db=low_db, channels=1) + signals.sine(6000.0, sr, dur, amp_db=high_db, channels=1)).astype(np.float32)


def levels(y, x, sr, freqs, start_s=0.3):
    return {f: measure.level_at_db(y[:, 0], sr, f, start_s=start_s) - measure.level_at_db(x[:, 0], sr, f, start_s=start_s) for f in freqs}


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("mode", MODES)
def test_deesser_only_band_reduced(sr, mode, record_property):
    x = two_tones(sr)
    y, rep = render_plugin(PLUGIN, sr=sr, params=for_mode(mode), input=x)
    assert rep["nan_count"] == 0
    d = levels(y, x, sr, [1000.0, 6000.0])
    record_property("delta_1k_db", float(d[1000.0]))
    record_property("delta_6k_db", float(d[6000.0]))
    assert abs(d[1000.0]) < 0.05  # the tone below the band is untouched
    assert d[6000.0] < -6.0       # the sibilant band is reduced (18 dB over the threshold)


@pytest.mark.parametrize("mode", MODES)
def test_deesser_no_reduction_below_threshold(mode):
    sr = 48000
    x = two_tones(sr, high_db=-40.0)  # 16 dB below the threshold
    y, rep = render_plugin(PLUGIN, sr=sr, params=for_mode(mode), input=x)
    assert rep["nan_count"] == 0
    if mode == "Band":
        assert measure.residual_dbfs(y, x) < -100.0  # the bell is the identity
    else:
        d = levels(y, x, sr, [1000.0, 6000.0])
        assert max(abs(v) for v in d.values()) < 0.02  # LR4 split sums to an all-pass: magnitude-flat


@pytest.mark.parametrize("mode", MODES)
def test_deesser_range_cap(mode, record_property):
    sr = 48000
    x = two_tones(sr, high_db=0.0)  # 24 dB over the threshold
    # Split: crossover 1.6 octaves below the sibilance so the low band's leakage (-38 dB) cannot mask the cap
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(for_mode(mode, range_db=6.0), **({"freq_hz": 2000.0} if mode == "Split" else {})), input=x)
    d = levels(y, x, sr, [6000.0])
    record_property("reduction_db", float(-d[6000.0]))
    assert abs(-d[6000.0] - 6.0) < 0.5


def test_deesser_split_sum_flat():
    sr = 48000
    x = signals.noise(sr, 1.0, seed=21, amp_db=-30.0, channels=1)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mode=1, threshold_db=0.0), input=x)
    for f in (50.0, 500.0, 2000.0, 6000.0, 12000.0, 18000.0):
        xi = signals.sine(f, sr, 0.5, amp_db=-20.0, channels=1)
        yi, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mode=1, threshold_db=0.0), input=xi)
        assert abs(levels(yi, xi, sr, [f], start_s=0.2)[f]) < 0.02


@pytest.mark.parametrize("mode", MODES)
def test_deesser_listen_is_difference(mode):
    sr = 48000
    x = two_tones(sr)
    wet, _ = render_plugin(PLUGIN, sr=sr, params=for_mode(mode), input=x)
    lis, _ = render_plugin(PLUGIN, sr=sr, params=for_mode(mode, listen=1), input=x)
    assert measure.residual_dbfs(lis, x - wet) < -100.0


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("lookahead_ms", [0.0, 1.0, 3.0])
def test_deesser_lookahead_latency(sr, lookahead_ms):
    x = signals.noise(sr, 0.5, seed=22, amp_db=-30.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, threshold_db=0.0, lookahead_ms=lookahead_ms), input=x)
    assert rep["latency_samples"] == round(lookahead_ms * sr / 1000)
    xc = np.correlate(y[:, 0], x[:, 0], mode="full")
    assert int(np.argmax(xc)) - (len(x) - 1) == rep["latency_samples"]


def test_deesser_lookahead_change_no_click():
    sr = 48000
    x = signals.sine(1100.0, sr, 1.5, amp_db=-12.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, threshold_db=0.0), input=x, automation=[{"param": "lookahead_ms", "t_s": 0.5, "value": 3.0}])
    assert rep["nan_count"] == 0
    assert rep["latency_samples"] == 144
    seg = slice(int(0.4 * sr), int(1.4 * sr))
    assert np.max(np.abs(np.diff(y[seg, 0]))) < 1.5 * np.max(np.abs(np.diff(x[seg, 0])))


def test_deesser_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=12.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE, tol_dbfs=-60.0, settle_s=0.3))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
