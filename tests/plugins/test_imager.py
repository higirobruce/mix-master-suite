import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module, render_plugin
from tests.plugins import base_checks

PLUGIN = "imager"
BASE = {"xover_low_hz": 150.0, "xover_high_hz": 3000.0, "width_low_pct": 100.0, "width_mid_pct": 100.0, "width_high_pct": 100.0,
        "pan": 0.0, "mono_below_hz": 0.0, "output_db": 0.0}
BANDS = {"width_low_pct": 40.0, "width_mid_pct": 800.0, "width_high_pct": 12000.0}  # a tone >= 2 octaves inside each band


def stereo_noise(sr, seconds=1.0, seed=31):
    """Two independent noises: a wide, decorrelated stereo source (correlation ~0)."""
    l = signals.noise(sr, seconds, seed=seed, amp_db=-18.0, channels=1)
    r = signals.noise(sr, seconds, seed=seed + 100, amp_db=-18.0, channels=1)
    return np.concatenate([l, r], axis=1).astype(np.float32)


def allpassed(x, sr, params):
    """The LR4 all-pass reference: both crossovers' low + high sum, applied to each channel (what width 100 outputs)."""
    y = x
    for f in (params["xover_low_hz"], params["xover_high_hz"]):
        y, _ = render_module("lr_crossover", sr=sr, params={"freq_hz": f, "order": 4, "output": 2}, input=y, channels=2)
    return y


def correlation(y):
    return float(np.corrcoef(y[:, 0], y[:, 1])[0, 1])


def test_imager_width_0_is_mono():
    sr = 48000
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_low_pct=0.0, width_mid_pct=0.0, width_high_pct=0.0), input=stereo_noise(sr))
    assert rep["nan_count"] == 0
    assert abs(correlation(y[int(0.2 * sr):]) - 1.0) < 0.001


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_imager_width_100_flat_and_coherent(sr):
    x = stereo_noise(sr)
    y, rep = render_plugin(PLUGIN, sr=sr, params=BASE, input=x)
    assert rep["nan_count"] == 0 and rep["latency_samples"] == 0
    ref = allpassed(x, sr, BASE)
    assert measure.residual_dbfs(y[int(0.1 * sr):], ref[int(0.1 * sr):]) < -100.0  # the all-passed input, sample-exact
    for f in (50.0, 400.0, 1000.0, 5000.0, 15000.0):
        xi = signals.sine(f, sr, 0.5, amp_db=-20.0, channels=2)
        yi, _ = render_plugin(PLUGIN, sr=sr, params=BASE, input=xi)
        assert abs(measure.level_at_db(yi[:, 0], sr, f, start_s=0.2) - measure.level_at_db(xi[:, 0], sr, f, start_s=0.2)) < 0.02


@pytest.mark.parametrize("band,freq", BANDS.items())
def test_imager_width_200_side_gain(band, freq, record_property):
    # a pure side signal (L = -R) in one band doubled by that band's width: +6.02 dB; the other bands' widths at 0
    # must not touch it (the tone sits >= 2 octaves inside its band: the LR4 edge is -0.22 dB at 1.3 octaves)
    sr = 48000
    x = signals.sine(freq, sr, 1.0, amp_db=-20.0, channels=2)
    x[:, 1] *= -1.0
    widths = {b: 0.0 for b in BANDS}
    widths[band] = 200.0
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, **widths), input=x)
    gain = measure.level_at_db(y[:, 0], sr, freq, start_s=0.3) - measure.level_at_db(x[:, 0], sr, freq, start_s=0.3)
    record_property(f"side_gain_{band}", float(gain))
    assert abs(gain - 6.02) < 0.1


def test_imager_mono_below(record_property):
    sr = 48000
    # LR4 (24 dB/oct) on the side: a pure side tone 2.9 octaves below the mono frequency is gone (< -60 dB); the
    # tone at 1.7 octaves is recorded (about -42 dB)
    for f, mono, limit in ((40.0, 300.0, -60.0), (60.0, 200.0, -36.0)):
        x = signals.sine(f, sr, 1.0, amp_db=-20.0, channels=2)
        x[:, 1] *= -1.0  # pure side
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mono_below_hz=mono), input=x)
        att = measure.level_at_db(y[:, 0], sr, f, start_s=0.3) - measure.level_at_db(x[:, 0], sr, f, start_s=0.3)
        record_property(f"side_attenuation_{int(f)}hz_mono_{int(mono)}", float(att))
        assert att < limit
    xh = signals.sine(2000.0, sr, 1.0, amp_db=-20.0, channels=2)
    xh[:, 1] *= -1.0
    yh, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mono_below_hz=200.0), input=xh)
    assert abs(measure.level_at_db(yh[:, 0], sr, 2000.0, start_s=0.3) - measure.level_at_db(xh[:, 0], sr, 2000.0, start_s=0.3)) < 0.05


@pytest.mark.parametrize("width", [0.0, 50.0, 150.0, 200.0])
def test_imager_mono_sum_flat(width):
    # mono compatibility: L + R does not change with width (M is never scaled), in every band
    sr = 48000
    x = stereo_noise(sr)
    ref = allpassed(x, sr, BASE)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_low_pct=width, width_mid_pct=width, width_high_pct=width), input=x)
    for f in BANDS.values():
        a = measure.level_at_db(y[:, 0] + y[:, 1], sr, f, start_s=0.2)
        b = measure.level_at_db(ref[:, 0] + ref[:, 1], sr, f, start_s=0.2)
        assert abs(a - b) < 0.1


def test_imager_pan():
    sr = 48000
    x = signals.sine(1000.0, sr, 0.5, amp_db=-20.0, channels=2)
    lvl = lambda y, ch: measure.level_at_db(y[:, ch], sr, 1000.0, start_s=0.2)
    left, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, pan=-100.0), input=x)
    right, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, pan=100.0), input=x)
    half, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, pan=50.0), input=x)
    assert lvl(left, 1) < -80.0 and lvl(right, 0) < -80.0
    assert abs(lvl(left, 0) - (lvl(x, 0) + 3.01)) < 0.1  # constant power: full pan = +3 dB on the kept side
    assert abs((10 ** (lvl(half, 0) / 10) + 10 ** (lvl(half, 1) / 10)) - 2 * 10 ** (lvl(x, 0) / 10)) / (2 * 10 ** (lvl(x, 0) / 10)) < 0.03


def test_imager_mono_input():
    # Review Focus 4: a mono input has no side: every width gives the all-passed input; pan still moves it
    sr = 48000
    x = signals.noise(sr, 0.5, seed=5, amp_db=-18.0, channels=1)
    ref = allpassed(np.concatenate([x, x], axis=1), sr, BASE)
    for w in (0.0, 200.0):
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_low_pct=w, width_mid_pct=w, width_high_pct=w), input=x, layout={"in": 1, "out": 2})
        assert measure.residual_dbfs(y[int(0.1 * sr):], ref[int(0.1 * sr):]) < -100.0
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, pan=100.0), input=x, layout={"in": 1, "out": 2})
    assert np.max(np.abs(y[:, 0])) < 1e-4


def test_imager_width_sweep_no_click():
    # Review Focus 3: width automated 0 -> 200 while a stereo tone runs: no sample jump above 2x the input's own
    sr = 48000
    x = signals.sine(440.0, sr, 2.0, amp_db=-12.0, channels=2)
    x[:, 1] = signals.sine(660.0, sr, 2.0, amp_db=-12.0, channels=1)[:, 0]
    auto = [{"param": b, "t_s": 0.5 + 0.01 * k, "value": 200.0 * k / 100} for k in range(101) for b in BANDS]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_low_pct=0.0, width_mid_pct=0.0, width_high_pct=0.0), input=x, automation=auto)
    assert rep["nan_count"] == 0
    wide, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, width_low_pct=200.0, width_mid_pct=200.0, width_high_pct=200.0), input=x)
    assert np.max(np.abs(np.diff(y[:, 0]))) < 1.1 * np.max(np.abs(np.diff(wide[:, 0])))  # the width-200 output's own slope


def test_imager_mono_below_switch_no_click():
    # mono-below switched on (0 -> 200 Hz), dragged to 400 Hz and off while a wide low tone plays: engaged by a
    # 20 ms crossfade and glided while on, so no sample jump above 2x the input's own
    sr = 48000
    x = signals.sine(80.0, sr, 2.0, amp_db=-12.0, channels=2)
    x[:, 1] *= -1.0
    auto = [{"param": "mono_below_hz", "t_s": 0.5, "value": 200.0}, {"param": "mono_below_hz", "t_s": 0.9, "value": 400.0},
            {"param": "mono_below_hz", "t_s": 1.2, "value": 0.0}]
    y, rep = render_plugin(PLUGIN, sr=sr, params=BASE, input=x, automation=auto)
    assert rep["nan_count"] == 0
    assert np.max(np.abs(np.diff(y[:, 0]))) < 2.0 * np.max(np.abs(np.diff(x[:, 0])))


@pytest.mark.parametrize("block", [512, 2048])
def test_imager_mono_below_drag_large_blocks(block):
    # review finding: the 5 ms/octave glide was applied once per block, so a drag clicked at 512 and 2048
    sr = 48000
    x = signals.sine(80.0, sr, 2.0, amp_db=-12.0, channels=2)
    x[:, 1] *= -1.0
    auto = [{"param": "mono_below_hz", "t_s": 0.5, "value": 500.0}, {"param": "mono_below_hz", "t_s": 1.2, "value": 20.0}]  # a fast drag
    y, rep = render_plugin(PLUGIN, sr=sr, block=block, params=dict(BASE, mono_below_hz=20.0), input=x, automation=auto)
    assert rep["nan_count"] == 0
    assert np.max(np.abs(np.diff(y[:, 0]))) < 2.0 * np.max(np.abs(np.diff(x[:, 0])))


def test_imager_pan_rotates_stereo_content():
    # review finding: pan scaled L and R independently (a balance control: a hard-right source vanished at pan
    # left). Pan is a constant-power stereo rotation: a hard-right source panned fully left lands in the centre at
    # equal level in both channels, -3 dB each, power kept; mono sources keep the sin/cos law (test_imager_pan)
    sr = 48000
    x = np.zeros((int(0.5 * sr), 2), dtype=np.float32)
    x[:, 1] = signals.sine(1000.0, sr, 0.5, amp_db=-20.0, channels=1)[:, 0]
    lvl = lambda y, ch: measure.level_at_db(y[:, ch], sr, 1000.0, start_s=0.2)
    power = lambda y: 10 ** (lvl(y, 0) / 10) + 10 ** (lvl(y, 1) / 10)
    centre, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, pan=-100.0), input=x)
    assert abs(lvl(centre, 0) - lvl(centre, 1)) < 0.05
    assert abs(lvl(centre, 0) - (lvl(x, 1) - 3.01)) < 0.1
    half, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, pan=-50.0), input=x)
    assert abs(power(half) - power(x)) / power(x) < 0.03
    assert lvl(half, 0) > lvl(x, 1) - 12.0  # the source has moved into the left channel


def test_imager_crossover_drag_no_click():
    # both crossovers dragged across a stereo tone pair at full width spread: no sample jump above 1.1x the slope of
    # the static output at either end of the drag
    sr = 48000
    x = signals.sine(300.0, sr, 2.0, amp_db=-12.0, channels=2)
    x[:, 1] = signals.sine(2500.0, sr, 2.0, amp_db=-12.0, channels=1)[:, 0]
    widths = dict(width_low_pct=0.0, width_mid_pct=200.0, width_high_pct=0.0)
    auto = [{"param": "xover_low_hz", "t_s": 0.5 + 0.01 * k, "value": 150.0 + 350.0 * k / 50} for k in range(51)]
    auto += [{"param": "xover_high_hz", "t_s": 0.5 + 0.01 * k, "value": 3000.0 - 1500.0 * k / 50} for k in range(51)]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, **widths), input=x, automation=auto)
    assert rep["nan_count"] == 0
    bound = 0.0  # the louder of the two static end positions sets the legitimate slope
    for xl, xh in ((150.0, 3000.0), (500.0, 1500.0)):
        static, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, **widths, xover_low_hz=xl, xover_high_hz=xh), input=x)
        bound = max(bound, float(np.max(np.abs(np.diff(static[int(0.3 * sr):, 0])))))
    assert np.max(np.abs(np.diff(y[:, 0]))) < 1.1 * bound


def test_imager_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=18.5)  # width 200 + output +12 on a -12 dBFS tone
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE, tol_dbfs=-80.0, settle_s=0.1))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
