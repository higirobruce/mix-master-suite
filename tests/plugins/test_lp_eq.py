"""Linear-phase EQ (M5 Task 3): response of linear and minimum-phase bands against the matched-biquad reference,
flat null through the FIR delay, latency per FIR length, click-free length changes, M/S band isolation, base checks,
CPU budget."""
from __future__ import annotations

import numpy as np
import pytest

from tests.harness import designs, measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "lp_eq"
TYPES = {"Bell": 0, "Low shelf": 1, "High shelf": 2, "Low cut": 3, "High cut": 4, "Notch": 5}
LENGTHS = [2048, 4096, 8192, 16384]
ALL_OFF = {f"b{n}_enabled": 0 for n in range(1, 9)}


def band(n: int, type_name: str, hz: float, gain: float = 0.0, q: float = 1.0, phase: int = 1, ms: int = 0) -> dict:
    return {f"b{n}_enabled": 1, f"b{n}_type": TYPES[type_name], f"b{n}_freq_hz": hz, f"b{n}_gain_db": gain,
            f"b{n}_q": q, f"b{n}_phase": phase, f"b{n}_ms": ms}


def reference_db(bands: list[tuple[str, float, float, float]], sr: float, f: np.ndarray) -> np.ndarray:
    """Magnitude of the band product: matched (Vicanek) designs for bell / shelves / cuts, RBJ for the notch."""
    db = np.zeros_like(f)
    for type_name, hz, gain, q in bands:
        kind = {"Bell": "peak", "Low shelf": "lowshelf", "High shelf": "highshelf", "Low cut": "highpass", "High cut": "lowpass"}.get(type_name)
        b, a = designs.matched(kind, sr, hz, gain, q) if kind else designs.rbj("notch", sr, hz, 0.0, q)
        db += designs.freqz_db_deg(b, a, f, sr)[0]
    return db


def impulse_response(sr: int, params: dict, length: int, n: int = 65536):
    """The whole symmetric impulse (length - 1 taps) centred on the reported latency, and the centre index within it."""
    x = signals.impulse(sr, n / sr, channels=2)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    centre = (length - 2) // 2
    start = rep["latency_samples"] - centre
    assert start >= 0
    return y[start:start + length - 1, 0].astype(np.float64), centre, rep


def spectrum(ir: np.ndarray, sr: int, f: np.ndarray, centre: int = 0) -> np.ndarray:
    """DFT referenced to `centre`: a linear-phase impulse gives a real, positive response."""
    n = np.arange(len(ir)) - centre
    return np.array([np.sum(ir * np.exp(-2j * np.pi * fi * n / sr)) for fi in f])


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("length_index", [1, 3])
def test_lp_eq_response_linear(sr, length_index, record_property):
    """Three linear-phase bands: |H| within 0.1 dB of the matched product above twice the resolution floor, and the
    phase linear within 2 deg (the response after the reported latency is real and positive)."""
    bands = [("Bell", 1000.0, 6.0, 1.0), ("Low shelf", 300.0, 4.0, 0.7), ("High cut", 12000.0, 0.0, 0.7)]
    params = dict(ALL_OFF, fir_length=length_index)
    for i, (t, hz, g, q) in enumerate(bands, start=1):
        params.update(band(i, t, hz, g, q, phase=1))
    length = LENGTHS[length_index]
    ir, centre, _ = impulse_response(sr, params, length)
    floor = 2.0 * sr / length
    f = np.geomspace(2.0 * floor, 0.45 * sr, 120)
    h = spectrum(ir, sr, f, centre)
    got = 20 * np.log10(np.abs(h))
    ref = reference_db(bands, sr, f)
    worst = float(np.max(np.abs(got - ref)))
    record_property("max_error_db", worst)
    assert worst < 0.1, f"{sr} Hz length {length}: max error {worst:.3f} dB"
    # the IR was taken from the reported latency, i.e. from the centre of the symmetric impulse: the response there
    # is real and positive for a linear-phase filter
    phase = np.degrees(np.angle(h))
    assert np.max(np.abs(phase)) < 2.0, f"phase not linear: {np.max(np.abs(phase)):.2f} deg"


@pytest.mark.parametrize("sr", [44100, 96000])
def test_lp_eq_minimum_phase_matches_channel_eq(sr):
    """The same bell / shelf in Minimum mode nulls against the Channel EQ (matched designs, 64-bit) < -120 dBFS
    once the FIR delay is removed."""
    x = signals.noise(sr, 1.0, seed=3)
    params = dict(ALL_OFF, **band(1, "Bell", 1000.0, 6.0, 1.0, phase=0), **band(2, "Low shelf", 200.0, -4.0, 0.7, phase=0))
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    ref_params = {"hp_enabled": 0, "lp_enabled": 0, "b1_enabled": 1, "b1_type": 0, "b1_freq_hz": 1000.0, "b1_gain_db": 6.0, "b1_q": 1.0,
                  "b2_enabled": 1, "b2_type": 1, "b2_freq_hz": 200.0, "b2_gain_db": -4.0, "b2_q": 0.7,
                  "b3_enabled": 0, "b4_enabled": 0, "b5_enabled": 0, "q_mode": 0, "oversample": 0}
    yref, _ = render_plugin("channel_eq", sr=sr, params=ref_params, input=x)
    lat = rep["latency_samples"]
    n = len(x) - lat
    assert measure.residual_dbfs(y[lat:lat + n], yref[:n]) < -120.0


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("length_index", range(4))
def test_lp_eq_flat_null_and_latency(sr, length_index, record_property):
    """Every band off: the output is the input delayed by the reported latency (< -120 dBFS), and the latency is the
    partition plus the FIR centre for that length."""
    x = signals.noise(sr, 1.0, seed=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(ALL_OFF, fir_length=length_index), input=x)
    lat = rep["latency_samples"]
    assert lat >= 512 + (LENGTHS[length_index] - 2) // 2
    assert lat <= 4096 + (LENGTHS[length_index] - 2) // 2
    n = len(x) - lat
    res = measure.residual_dbfs(y[lat:lat + n], x[:n])
    record_property("flat_residual_dbfs", res)
    assert res < -120.0
    # the impulse lands at the reported latency exactly
    imp = signals.impulse(sr, 0.5, channels=2)
    yi, rep2 = render_plugin(PLUGIN, sr=sr, params=dict(ALL_OFF, fir_length=length_index), input=imp)
    assert rep2["latency_samples"] == lat
    assert int(np.argmax(np.abs(yi[:, 0]))) == lat


def test_lp_eq_length_change_no_click(record_property):
    """FIR length 2048 -> 16384 while a tone plays: the gain envelope has no step above 2x the static output's own
    slope (the two impulses crossfade), and the band curve is unchanged after the switch."""
    sr = 48000
    x = signals.sine(300.0, sr, 3.0, amp_db=-12)
    params = dict(ALL_OFF, **band(1, "Bell", 300.0, 6.0, 1.0), fir_length=0)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x, automation=[{"param": "fir_length", "t_s": 1.5, "value": 3}])
    assert rep["nan_count"] == 0
    # the largest sample step across the change against the static output's own (0.8-1.3 s, before the change)
    static = float(np.max(np.abs(np.diff(y[int(0.8 * sr):int(1.3 * sr), 0]))))
    step = float(np.max(np.abs(np.diff(y[int(1.3 * sr):int(2.9 * sr), 0])))) / static
    record_property("length_change_step_ratio", step)
    assert step < 1.1
    # level after the switch (+6 dB on the 300 Hz tone) within 0.1 dB of before
    before = measure.rms_db(y[int(0.8 * sr):int(1.3 * sr), 0])
    after = measure.rms_db(y[int(2.4 * sr):int(2.9 * sr), 0])
    assert abs(before - after) < 0.1


@pytest.mark.parametrize("ms,which", [(1, "mid"), (2, "side")])
def test_lp_eq_ms_band(ms, which):
    """A Mid-only bell leaves a pure-side input untouched and a Side-only bell leaves a mono input untouched
    (< -120 dBFS after the delay); the band it targets reads its gain."""
    sr = 48000
    tone = signals.sine(1000.0, sr, 1.0, amp_db=-12, channels=1)[:, 0]
    mono = np.stack([tone, tone], axis=1).astype(np.float32)
    side = np.stack([tone, -tone], axis=1).astype(np.float32)
    params = dict(ALL_OFF, **band(1, "Bell", 1000.0, 6.0, 1.0, phase=1, ms=ms))
    untouched, targeted = (side, mono) if which == "mid" else (mono, side)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=untouched)
    lat = rep["latency_samples"]
    n = len(untouched) - lat
    assert measure.residual_dbfs(y[lat:lat + n], untouched[:n]) < -120.0
    y2, _ = render_plugin(PLUGIN, sr=sr, params=params, input=targeted)
    gain = measure.level_at_db(y2[:, 0], sr, 1000.0, start_s=0.5) - measure.level_at_db(targeted[:, 0], sr, 1000.0, start_s=0.5)
    assert abs(gain - 6.0) < 0.1


def test_lp_eq_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=24.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=ALL_OFF))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=ALL_OFF, tol_dbfs=-100.0, settle_s=0.5))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=ALL_OFF)
    base_checks.check_channel_configs(PLUGIN, params=ALL_OFF)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
