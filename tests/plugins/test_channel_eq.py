import numpy as np
import pytest

from tests.harness import eq_ref, measure, signals
from tests.harness.matrix import SAMPLE_RATES, assert_response, measure_band
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "channel_eq"
FLAT = {f"b{n}_gain_db": 0.0 for n in range(1, 6)}


def impulse_response(sr, params, n=65536):
    x = signals.impulse(sr, n / sr, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    return y[:, 0].astype(np.float64), rep


def spectrum(ir, sr, f):
    n = np.arange(len(ir))
    return np.array([np.sum(ir * np.exp(-2j * np.pi * fi * n / sr)) for fi in f])


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_eq_null_flat(sr, record_property):
    x = signals.noise(sr, 1.0, seed=31)
    y, rep = render_plugin(PLUGIN, sr=sr, params=FLAT, input=x)
    res = measure.residual_dbfs(y, x)
    record_property("null_residual_dbfs", res)
    assert rep["latency_samples"] == 0
    assert res < -120


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("band_type", ["Bell", "LowShelf", "HighShelf"])
@pytest.mark.parametrize("f0", [60.0, 250.0, 1000.0, 4000.0, 12000.0])
@pytest.mark.parametrize("gain", [-12.0, 6.0])
def test_eq_band_response(sr, band_type, f0, gain, record_property):
    if f0 > 0.45 * sr:
        pytest.skip("above the measurement band")
    q = 2.0 if band_type == "Bell" else 0.7
    params = dict(FLAT, b3_type=["Bell", "LowShelf", "HighShelf"].index(band_type), b3_freq_hz=f0, b3_gain_db=gain, b3_q=q)
    ir, _ = impulse_response(sr, params)
    lo, hi = measure_band(sr)
    f = np.logspace(np.log10(lo), np.log10(hi), 50)
    h = spectrum(ir, sr, f)
    ref = eq_ref.response(params, sr, f)
    d = assert_response(f, 20 * np.log10(np.abs(h) + 1e-300), np.degrees(np.angle(h)),
                        20 * np.log10(np.abs(ref) + 1e-300), np.degrees(np.angle(ref)), (lo, hi),
                        label=f"{band_type} {f0} Hz {gain} dB @ {sr}")
    record_property("max_dev_db", d[0])
    record_property("max_dev_deg", d[1])


@pytest.mark.parametrize("sr", [48000, 96000])
@pytest.mark.parametrize("slope", range(6))
def test_eq_hp_lp_slopes(sr, slope):
    for side, fc in (("hp", 200.0), ("lp", 5000.0)):
        params = dict(FLAT, **{f"{side}_enabled": 1, f"{side}_freq_hz": fc, f"{side}_slope": slope})
        ir, _ = impulse_response(sr, params)
        lo, hi = measure_band(sr)
        f = np.logspace(np.log10(lo), np.log10(hi), 60)
        h = spectrum(ir, sr, f)
        ref = eq_ref.response(params, sr, f)
        keep = 20 * np.log10(np.abs(ref) + 1e-300) > -100
        assert_response(f[keep], 20 * np.log10(np.abs(h[keep]) + 1e-300), np.degrees(np.angle(h[keep])),
                        20 * np.log10(np.abs(ref[keep]) + 1e-300), np.degrees(np.angle(ref[keep])), (lo, hi),
                        label=f"{side} slope {eq_ref.SLOPES[slope]} @ {sr}")
        at_fc = 20 * np.log10(abs(spectrum(ir, sr, [fc])[0]))
        assert abs(at_fc - (-3.01)) < 0.15  # Butterworth cascades are -3 dB at fc


def test_eq_analog_q_narrows_bell():
    sr = 48000
    def bw(q_mode):
        ir, _ = impulse_response(sr, dict(FLAT, b3_type=0, b3_freq_hz=1000.0, b3_gain_db=12.0, b3_q=1.0, q_mode=q_mode))
        f = np.logspace(np.log10(200), np.log10(5000), 2000)
        mag = 20 * np.log10(np.abs(spectrum(ir, sr, f)))
        above = f[mag > 9.0]  # -3 dB from the +12 peak
        return above[-1] - above[0]  # bandwidth in Hz: exactly proportional to 1/Q for a peaking filter
    ratio = bw(0) / bw(1)
    assert 1.8 < ratio < 2.2  # Q_eff = Q (1 + 12/12) = 2 Q → half the bandwidth, within 10 %


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_eq_oversample_latency_and_response(sr):
    params = dict(FLAT, b3_gain_db=6.0, b3_freq_hz=5000.0, b3_q=1.0, oversample=1)
    ir2, rep = impulse_response(sr, params)
    ir1, rep1 = impulse_response(sr, dict(params, oversample=0))
    assert rep1["latency_samples"] == 0
    assert rep["latency_samples"] > 0
    flat2, rep_flat = impulse_response(sr, dict(FLAT, oversample=1))  # flat chain: the IR peak is the latency
    assert int(np.argmax(np.abs(flat2))) == rep_flat["latency_samples"] == rep["latency_samples"]
    from tests.harness import designs
    f = np.logspace(np.log10(50), np.log10(0.4 * sr), 40)
    h1 = 20 * np.log10(np.abs(spectrum(ir1, sr, f)))
    h2 = 20 * np.log10(np.abs(spectrum(ir2, sr, f)))
    analog = 10 * np.log10(designs.analog_prototype("peak", 5000.0, 6.0, 1.0, f))
    # what 2x buys: the oversampled path tracks the analog curve within 0.1 dB up to 0.4 fs (the 1x matched design
    # is recorded for comparison; it drifts by ~0.1 dB near 0.3 fs at 44.1 k)
    assert np.max(np.abs(h2 - analog)) < 0.1
    assert np.max(np.abs(h1 - h2)) < 0.25


def test_eq_band_sweep_stability():
    sr = 48000
    x = signals.noise(sr, 2.0, seed=32, amp_db=-12)
    auto = [{"param": "b3_freq_hz", "t_s": t, "value": 20.0 * (1000.0 ** (t / 2.0))} for t in np.linspace(0, 2.0, 400)]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(FLAT, b3_gain_db=12.0, b3_q=6.0), input=x, automation=auto)
    assert rep["nan_count"] == 0
    assert rep["peak_db"] < 24.0


def test_variable_block_sizes(record_property):
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=dict(FLAT, b3_gain_db=6.0)))


def test_sr_change_mid_stream(record_property):
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=dict(FLAT, b3_gain_db=6.0), tol_dbfs=-100.0))


def test_state_roundtrip():
    base_checks.check_state_roundtrip(PLUGIN)


def test_param_extremes():
    # 18 dB over the 20 ms ramp is 0.9 dB per ms; a click would show as a far larger step in a 1 ms envelope
    base_checks.check_param_extremes(PLUGIN, max_peak_db=30.0, ramp_params={"b3_gain_db": 2.0}, ramp_window_ms=1.0)


def test_denormal_tail():
    base_checks.check_denormal_tail(PLUGIN, params=dict(FLAT, b3_gain_db=6.0, hp_enabled=1, lp_enabled=1))


def test_channel_configs():
    base_checks.check_channel_configs(PLUGIN)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
