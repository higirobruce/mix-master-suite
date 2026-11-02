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


@pytest.mark.parametrize("sr", SAMPLE_RATES)
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
        assert abs(at_fc - (-3.01)) < 0.1  # Butterworth cascades are -3 dB at fc


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


def test_eq_band_sweep_stability(record_property):
    # a +12 dB bell swept 20 Hz -> 20 kHz in 2 s across a 1 kHz tone (and across band 2 at 250 Hz): finite, bounded,
    # and the 1 ms gain envelope never steps more than 0.5 dB (the bell's own passage is ~0.2 dB/ms at Q 2)
    sr = 48000
    x = signals.sine(1000, sr, 2.0, amp_db=-12, channels=1)
    auto = [{"param": "b3_freq_hz", "t_s": t, "value": 20.0 * (1000.0 ** (t / 2.0))} for t in np.linspace(0, 2.0, 400)]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(FLAT, b3_gain_db=12.0, b3_q=2.0, b2_gain_db=6.0), input=x, automation=auto)
    assert rep["nan_count"] == 0
    assert rep["peak_db"] < 24.0
    step = measure.envelope_step_db(y[:, 0], x[:, 0], sr, start_s=0.05)
    record_property("sweep_envelope_step_db", step)
    assert step < 0.5


@pytest.mark.parametrize("slope", range(6))
@pytest.mark.parametrize("side", ["hp", "lp"])
def test_eq_hp_lp_cutoff_sweep_click(side, slope, record_property):
    # cutoff swept through a decade over 2 s on a 1 kHz tone, at block 512: no click (1 ms envelope step < 0.5 dB)
    sr = 48000
    x = signals.sine(1000, sr, 2.0, amp_db=-12, channels=1)
    lo, hi = (20.0, 400.0) if side == "hp" else (20000.0, 2500.0)
    auto = [{"param": f"{side}_freq_hz", "t_s": t, "value": lo * ((hi / lo) ** (t / 2.0))} for t in np.linspace(0, 2.0, 400)]
    params = dict(FLAT, **{f"{side}_enabled": 1, f"{side}_freq_hz": lo, f"{side}_slope": slope})
    y, rep = render_plugin(PLUGIN, sr=sr, block=512, params=params, input=x, automation=auto)
    assert rep["nan_count"] == 0
    step = measure.envelope_step_db(y[:, 0], x[:, 0], sr, start_s=0.05)
    record_property(f"{side}_{slope}_step_db", step)
    assert step < 0.5


@pytest.mark.parametrize("block", [64, 512, 2048])
def test_eq_band_to_zero_and_off_click(block, record_property):
    # a +12 dB bell on the tone ramped to exactly 0 dB, then switched off: the band must finish its ramp before it
    # drops out of the chain (the reviewer measured a -24 dBFS click at block 512)
    sr = 48000
    x = signals.sine(1000, sr, 1.5, amp_db=-12, channels=1)
    params = dict(FLAT, b3_freq_hz=1000.0, b3_gain_db=12.0, b3_q=1.0)
    y, rep = render_plugin(PLUGIN, sr=sr, block=block, params=params, input=x,
                           automation=[{"param": "b3_gain_db", "t_s": 0.5, "value": 0.0}])
    assert rep["nan_count"] == 0
    step_zero = measure.envelope_step_db(y[:, 0], x[:, 0], sr, start_s=0.05)
    y, rep = render_plugin(PLUGIN, sr=sr, block=block, params=params, input=x,
                           automation=[{"param": "b3_enabled", "t_s": 0.5, "value": 0}, {"param": "b3_enabled", "t_s": 1.0, "value": 1}])
    assert rep["nan_count"] == 0
    step_off = measure.envelope_step_db(y[:, 0], x[:, 0], sr, start_s=0.05)
    record_property("step_to_zero_db", step_zero)
    record_property("step_off_on_db", step_off)
    assert step_zero < 1.5  # 12 dB over the 20 ms ramp (~0.6 dB/ms; the matched-design lerp is not linear in dB)
    assert step_off < 1.5  # enable is a 20 ms gain glide to 0 dB, never a step


@pytest.mark.parametrize("slope", [1, 3, 5])
def test_eq_hp_toggle_and_slope_change_click(slope, record_property):
    # HP at 1 kHz on a 1 kHz tone (-3 dB, 90+ degrees): enable on -> off -> on and a slope change while running glide
    # the cutoff to the band edge over 20 ms, never a step (1 ms windows hold whole cycles: the envelope is exact)
    sr = 48000
    x = signals.sine(1000, sr, 2.0, amp_db=-12, channels=1)
    params = dict(FLAT, hp_enabled=1, hp_freq_hz=1000.0, hp_slope=slope)
    y, rep = render_plugin(PLUGIN, sr=sr, block=512, params=params, input=x,
                           automation=[{"param": "hp_enabled", "t_s": 0.5, "value": 0}, {"param": "hp_enabled", "t_s": 1.0, "value": 1},
                                       {"param": "hp_slope", "t_s": 1.5, "value": (slope + 2) % 6}])
    assert rep["nan_count"] == 0
    step = measure.envelope_step_db(y[:, 0], x[:, 0], sr, start_s=0.05)
    record_property(f"hp_toggle_slope_{slope}_step_db", step)
    assert step < 2.0  # the -3 dB at fc vanishes within the first octave of the 9-octave / 20 ms glide (~2 ms)


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
