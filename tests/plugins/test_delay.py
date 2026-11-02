import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "delay"
# wet only, no feedback, filters wide open, no modulation
BASE = {"time_l_ms": 375.0, "time_r_ms": 375.0, "link_lr": 0, "sync": 0, "sync_r": 0, "feedback_pct": 0.0, "ping_pong": 0,
        "fb_lp_hz": 20000.0, "fb_hp_hz": 20.0, "mod_depth_ms": 0.0, "mod_rate_hz": 0.5, "mix_pct": 100.0,
        "duck_amount_db": 0.0, "duck_release_ms": 300.0}


def impulse_taps(sr, params, dur=2.0, channel=0, min_db=-60.0, **job):
    """(time_s, level_db) of every echo of a unit impulse, from local maxima above min_db (1 ms apart)."""
    x = signals.impulse(sr, dur, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x, **job)
    assert rep["nan_count"] == 0
    a = np.abs(y[:, channel])
    taps = []
    i = 1
    w = max(2, int(sr * 0.001))
    while i < len(a) - 1:
        if a[i] > 10 ** (min_db / 20) and a[i] >= a[max(0, i - w):i + w].max():
            # refine with the parabolic peak of the interpolated echo
            taps.append((i / sr, 20 * np.log10(a[i])))
            i += w
        else:
            i += 1
    return taps, rep


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("time_ms", [1.0, 37.5, 375.0, 4000.0])
def test_delay_time_accuracy(sr, time_ms, record_property):
    taps, _ = impulse_taps(sr, dict(BASE, time_l_ms=time_ms, time_r_ms=time_ms), dur=time_ms / 1000 + 0.5)
    assert taps, "no echo found"
    t, lvl = taps[0]
    err_ms = (t - time_ms / 1000) * 1000
    record_property("time_error_ms", float(err_ms))
    record_property("echo_peak_db", float(lvl))  # a half-sample Lagrange delay spreads an impulse (peak down to -5 dB)
    assert abs(err_ms) < 0.05 + 1000.0 / sr  # within 0.05 ms plus the sample grid


@pytest.mark.parametrize("sync,bpm,expected_ms", [(4, 120.0, 250.0), (7, 120.0, 750.0), (11, 120.0, 166.667), (3, 90.0, 666.667)])
def test_delay_tempo_sync(sync, bpm, expected_ms):
    sr = 48000
    taps, _ = impulse_taps(sr, dict(BASE, sync=sync), dur=1.5, bpm=bpm)
    assert abs(taps[0][0] * 1000 - expected_ms) < 0.1


def test_delay_feedback_decay(record_property):
    sr = 48000
    taps, _ = impulse_taps(sr, dict(BASE, time_l_ms=100.0, time_r_ms=100.0, feedback_pct=50.0), dur=1.2)
    levels = [lvl for _, lvl in taps[:6]]
    assert len(levels) == 6
    assert abs(levels[0]) < 0.05  # 100 ms at 48 k is an integer delay: the first echo is the unit impulse itself
    expected = [20 * np.log10(0.5 ** n) for n in range(6)]
    errs = [abs(a - b) for a, b in zip(levels, expected)]
    record_property("feedback_tap_error_db_max", float(max(errs)))
    assert max(errs) < 0.2


def test_delay_feedback_soft_limit():
    # 120 % feedback on a burst: the loop self-oscillates but the soft limiter holds the peak at +6 dBFS; finite over 10 s
    sr = 48000
    x = signals.sine(1000.0, sr, 10.0, amp_db=-6.0, channels=1)
    x[int(0.2 * sr):] = 0.0
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, time_l_ms=50.0, time_r_ms=50.0, feedback_pct=120.0), input=x)
    assert rep["nan_count"] == 0
    assert rep["peak_db"] < 6.1
    assert 20 * np.log10(np.max(np.abs(y[int(8.0 * sr):, 0])) + 1e-12) > -20.0  # still ringing after 8 s


def test_delay_ping_pong_alternation():
    sr = 48000
    params = dict(BASE, time_l_ms=100.0, time_r_ms=100.0, feedback_pct=60.0, ping_pong=1)
    x = signals.impulse(sr, 1.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x, layout={"in": 1, "out": 2})
    assert rep["nan_count"] == 0
    for n in range(1, 6):
        i = int(n * 0.1 * sr)
        seg = slice(i - 48, i + 48)
        l, r = np.max(np.abs(y[seg, 0])), np.max(np.abs(y[seg, 1]))
        if n % 2 == 1:
            assert l > 10 * r  # odd taps on the left
        else:
            assert r > 10 * l  # even taps on the right


def test_delay_filter_in_loop():
    sr = 48000
    for freq, expect_attenuated in ((200.0, False), (4000.0, True)):
        x = signals.sine(freq, sr, 0.05, amp_db=-12.0, channels=1)
        x = np.concatenate([x, np.zeros((int(1.0 * sr), 1), dtype=np.float32)])
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, time_l_ms=100.0, time_r_ms=100.0, feedback_pct=70.0, fb_lp_hz=1000.0), input=x)
        tap5 = y[int(0.5 * sr):int(0.55 * sr), 0]
        lvl = 20 * np.log10(np.sqrt(np.mean(tap5 ** 2)) + 1e-12)
        if expect_attenuated:
            hi = lvl
        else:
            lo = lvl
    assert lo - hi > 20.0


def test_delay_modulation():
    # a click every 300 ms through a 100 ms delay with 5 ms depth at 1 Hz: echo times spread by about +-5 ms
    sr = 48000
    x = np.zeros((int(4.0 * sr), 1), dtype=np.float32)
    for k in range(12):
        x[int((0.1 + 0.3 * k) * sr), 0] = 1.0
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, time_l_ms=100.0, time_r_ms=100.0, mod_depth_ms=5.0, mod_rate_hz=1.0), input=x)
    assert rep["nan_count"] == 0
    a = np.abs(y[:, 0])
    offsets = []
    for k in range(12):
        c = int((0.2 + 0.3 * k) * sr)
        seg = a[c - int(0.012 * sr):c + int(0.012 * sr)]
        offsets.append((np.argmax(seg) - int(0.012 * sr)) / sr * 1000)
    spread = max(offsets) - min(offsets)
    assert 8.0 < spread < 11.0  # 2 x 5 ms within 10 %


def test_delay_ducking():
    sr = 48000
    # loud dry for 2 s then silence: the echo of a -12 dBFS tone is ducked while the dry plays, recovers within 3 releases
    x = signals.sine(1000.0, sr, 3.0, amp_db=-6.0, channels=1)
    x[int(2.0 * sr):] = 0.0
    p = dict(BASE, time_l_ms=50.0, time_r_ms=50.0, feedback_pct=85.0, mix_pct=100.0, duck_release_ms=200.0)
    y_off, _ = render_plugin(PLUGIN, sr=sr, params=dict(p, duck_amount_db=0.0), input=x)
    y_on, _ = render_plugin(PLUGIN, sr=sr, params=dict(p, duck_amount_db=12.0), input=x)
    during = measure.level_at_db(y_on[:, 0], sr, 1000.0, start_s=1.0, end_s=1.9) - measure.level_at_db(y_off[:, 0], sr, 1000.0, start_s=1.0, end_s=1.9)
    assert abs(during - -12.0) < 1.0
    after = measure.level_at_db(y_on[:, 0], sr, 1000.0, start_s=2.6, end_s=2.9) - measure.level_at_db(y_off[:, 0], sr, 1000.0, start_s=2.6, end_s=2.9)
    assert abs(after) < 1.0


@pytest.mark.parametrize("points", [2, 100, 300])
def test_delay_no_zipper_on_time_change(points):
    # Review Focus 2: time dragged 1 -> 4000 ms (log, `points` automation points over 1 s) and jumped back while a
    # tone runs: glides and tap crossfades keep every sample-to-sample jump within 2x the input's own
    sr = 48000
    x = signals.sine(440.0, sr, 2.5, amp_db=-12.0, channels=1)
    auto = [{"param": "time_l_ms", "t_s": 0.5 + 1.0 * k / max(1, points - 1), "value": 1.0 * (4000.0 ** (k / max(1, points - 1)))} for k in range(points)]
    auto.append({"param": "time_l_ms", "t_s": 1.8, "value": 1.0})
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, time_l_ms=1.0, time_r_ms=1.0, feedback_pct=40.0), input=x, automation=auto)
    assert rep["nan_count"] == 0
    assert np.max(np.abs(np.diff(y[:, 0]))) < 2.0 * np.max(np.abs(np.diff(x[:, 0])))


def test_delay_mono_output_sums_ping_pong():
    # mono in / mono out with ping-pong: every tap is heard (the mono sum of L and R), not only the left ones
    sr = 48000
    params = dict(BASE, time_l_ms=100.0, time_r_ms=100.0, feedback_pct=60.0, ping_pong=1)
    taps, _ = impulse_taps(sr, params, dur=1.0, layout={"in": 1, "out": 1})
    times = [round(t * 1000) for t, _ in taps[:6]]
    assert times == [100, 200, 300, 400, 500, 600]


def test_delay_independent_sync_sides():
    # spec: L/R independent under tempo sync: L dotted eighth (375 ms) and R quarter (500 ms) at 120 BPM
    sr = 48000
    params = dict(BASE, sync=8, sync_r=3)
    x = signals.impulse(sr, 1.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x, layout={"in": 1, "out": 2}, bpm=120.0)
    assert rep["nan_count"] == 0
    assert abs(int(np.argmax(np.abs(y[:, 0]))) / sr * 1000 - 375.0) < 0.1
    assert abs(int(np.argmax(np.abs(y[:, 1]))) / sr * 1000 - 500.0) < 0.1


def test_delay_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=12.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=dict(BASE, time_l_ms=20.0, time_r_ms=20.0), tol_dbfs=-60.0, settle_s=0.1))
    base_checks.check_state_roundtrip(PLUGIN)
    # a 20 ms loop at 50 % feedback decays 75 times in the 1.5 s silence: the tail must reach exact zero, not denormals
    base_checks.check_denormal_tail(PLUGIN, params=dict(BASE, time_l_ms=20.0, time_r_ms=20.0, feedback_pct=50.0))
    base_checks.check_channel_configs(PLUGIN, params=dict(BASE, time_l_ms=20.0, time_r_ms=20.0))  # the check renders 0.2 s


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
