import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "gate"
BASE = {"threshold_db": -30.0, "range_db": -40.0, "attack_ms": 1.0, "hold_ms": 0.0, "release_ms": 100.0, "mode": 0,
        "ratio": 4.0, "hysteresis_db": 0.0, "lookahead_ms": 0.0}


def gain_env_db(y, x, sr, win_ms=1.0):
    """Gain in dB per win_ms RMS window (sine-safe)."""
    w = max(1, int(sr * win_ms / 1000))
    n = (len(y) // w) * w
    ry = np.sqrt(np.mean(y[:n].reshape(-1, w) ** 2, axis=1))
    rx = np.sqrt(np.mean(x[:n].reshape(-1, w) ** 2, axis=1))
    return 20 * np.log10(ry / (rx + 1e-20) + 1e-20)


def tone_level_db(sr, in_db, **over):
    x = signals.sine(1000, sr, 1.0, amp_db=in_db, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, **over), input=x)
    assert rep["nan_count"] == 0
    return measure.level_at_db(y[:, 0], sr, 1000, start_s=0.6)


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("attack_ms,release_ms", [(0.1, 10.0), (1.0, 100.0), (10.0, 1000.0)])
def test_gate_open_close_times(sr, attack_ms, release_ms, record_property):
    n = int((0.6 + 4 * release_ms / 1000) * sr)
    t = np.arange(n) / sr
    x = np.where((t >= 0.1) & (t < 0.4), 1.0, 10 ** (-60 / 20)).astype(np.float32)[:, None]
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, attack_ms=attack_ms, release_ms=release_ms), input=x)
    assert rep["nan_count"] == 0
    g = 20 * np.log10(np.abs(y[:, 0]) / (np.abs(x[:, 0]) + 1e-12) + 1e-12)
    on = g[int(0.1 * sr):int(0.4 * sr)]
    off = g[int(0.4 * sr):]
    rng = 40.0  # range_db: closed floor
    assert abs(np.mean(off[-len(off) // 5:]) + rng) < 0.1
    assert abs(np.mean(on[-len(on) // 5:])) < 0.01
    atk = (np.argmax(on >= -0.1 * rng) - np.argmax(on >= -0.9 * rng)) / sr * 1000
    rel = (np.argmax(off <= -0.9 * rng) - np.argmax(off <= -0.1 * rng)) / sr * 1000
    record_property("attack_ms_measured", float(atk))
    record_property("release_ms_measured", float(rel))
    assert abs(atk - attack_ms) <= 0.1 * attack_ms + 1000.0 / sr
    assert abs(rel - release_ms) <= 0.1 * release_ms + 1000.0 / sr


@pytest.mark.parametrize("hold_ms", [50.0, 200.0, 1000.0])
def test_gate_hold(hold_ms, record_property):
    sr = 48000
    dur = 0.3 + 2 * hold_ms / 1000
    x = signals.sine(1000, sr, dur, amp_db=-70.0, channels=1)
    burst = (np.arange(len(x)) / sr >= 0.1) & (np.arange(len(x)) / sr < 0.2)
    x[burst] *= 10 ** (70 / 20)  # 100 ms burst at 0 dB, shorter than the hold
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, hold_ms=hold_ms, attack_ms=0.1, release_ms=10.0), input=x)
    assert rep["nan_count"] == 0
    g = gain_env_db(y[:, 0], x[:, 0], sr)
    after = g[201:]  # windows after the burst end (0.2 s), skipping the edge window
    closing = np.argmax(after <= -1.0)  # first window where the gate has started to release
    measured = closing / 1000 * 1000  # ms (1 ms windows)
    record_property("hold_ms_measured", float(measured))
    assert abs(measured - hold_ms) <= 0.1 * hold_ms + 1.0


@pytest.mark.parametrize("range_db", [-20.0, -40.0, -80.0])
def test_gate_range_floor(range_db):
    out = tone_level_db(48000, -60.0, range_db=range_db)  # 30 dB below threshold: closed
    assert abs(out - (-60.0 + range_db)) < 0.1


def test_gate_hysteresis_no_chatter(record_property):
    sr = 48000
    t = np.arange(int(3.0 * sr)) / sr
    level_db = -30.0 + 1.0 * np.sin(2 * np.pi * 5.0 * t)  # +-1 dB AM at 5 Hz around the threshold
    x = (10 ** (level_db / 20) * np.sin(2 * np.pi * 1000.0 * t)).astype(np.float32)[:, None]
    cycles = 3.0 * 5.0

    def transitions(hyst):
        y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, hysteresis_db=hyst, hold_ms=50.0, attack_ms=0.1, release_ms=10.0), input=x)
        assert rep["nan_count"] == 0
        g = gain_env_db(y[:, 0], x[:, 0], sr)
        closed = g < -6.0
        return int(np.sum(~closed[:-1] & closed[1:]))  # open -> closed transitions

    with_hyst, without = transitions(3.0), transitions(0.0)
    record_property("transitions_hyst_3db", with_hyst)
    record_property("transitions_hyst_0db", without)
    assert with_hyst <= cycles
    assert without > with_hyst  # hysteresis is what keeps the gate open through the dips


def test_gate_lookahead_latency(record_property):
    sr = 48000
    noise = signals.noise(sr, 0.5, seed=7, amp_db=-6.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, lookahead_ms=5.0, range_db=0.0), input=noise)
    assert rep["latency_samples"] == 240
    xc = np.correlate(y[:, 0], noise[:, 0], mode="full")
    assert int(np.argmax(xc)) - (len(noise) - 1) == 240  # measured delay == reported latency
    # onset: a 0 dB tone starting at 0.2 s after a -80 dB lead-in; with lookahead the gate is open at the onset
    x = signals.sine(1000, sr, 0.5, amp_db=0.0, channels=1)
    x[: int(0.2 * sr)] *= 10 ** (-80 / 20)
    onset = int(0.2 * sr)
    first_ms = slice(0, 48)

    def onset_ratio(lookahead_ms):
        y2, rep2 = render_plugin(PLUGIN, sr=sr, params=dict(BASE, lookahead_ms=lookahead_ms), input=x)
        lat = rep2["latency_samples"]
        out = y2[onset + lat:, 0][first_ms]
        ref = x[onset:, 0][first_ms]
        return float(np.sqrt(np.mean(out ** 2)) / np.sqrt(np.mean(ref ** 2)))

    with_la, without = onset_ratio(5.0), onset_ratio(0.0)
    record_property("onset_ratio_lookahead_5ms", with_la)
    record_property("onset_ratio_lookahead_0ms", without)
    assert with_la >= 0.9
    assert without < with_la


@pytest.mark.parametrize("ratio", [2.0, 4.0])
def test_gate_expander_ratio(ratio, record_property):
    sr = 48000
    over = dict(mode=1, threshold_db=-20.0, ratio=ratio, range_db=-80.0)
    outs = [tone_level_db(sr, L, **over) for L in (-30.0, -40.0)]
    slope = (outs[1] - outs[0]) / (-40.0 - -30.0)
    record_property(f"expander_ratio_{ratio}_measured", float(slope))
    assert abs(slope - ratio) < 0.1
    assert abs(tone_level_db(sr, -10.0, **over) - -10.0) < 0.05  # above threshold: untouched


def test_gate_sidechain_filters():
    sr = 48000
    low = signals.sine(100, sr, 1.0, amp_db=-10.0, channels=1)
    y_open, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE), input=low)
    y_hp, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, sc_hp_enabled=1, sc_hp_hz=1000.0), input=low)
    assert abs(measure.level_at_db(y_open[:, 0], sr, 100, start_s=0.6) - -10.0) < 0.1
    assert measure.level_at_db(y_hp[:, 0], sr, 100, start_s=0.6) < -40.0  # the HP removed the key: closed
    high = signals.sine(8000, sr, 1.0, amp_db=-10.0, channels=1)
    y_lp, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, sc_lp_enabled=1, sc_lp_hz=1000.0), input=high)
    assert measure.level_at_db(y_lp[:, 0], sr, 8000, start_s=0.6) < -40.0


def test_gate_ext_key():
    sr = 48000
    x = signals.sine(1000, sr, 1.0, amp_db=0.0, channels=1)
    quiet_key, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ext_key=1), input=x, layout={"in": 1, "out": 1, "sidechain": 1},
                                 sidechain_db=-80.0)
    assert measure.level_at_db(quiet_key[:, 0], sr, 1000, start_s=0.6) < -39.0  # silent key keeps the gate closed
    no_bus, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ext_key=1), input=x, layout={"in": 1, "out": 1, "sidechain": 0})
    assert rep["nan_count"] == 0
    assert abs(measure.level_at_db(no_bus[:, 0], sr, 1000, start_s=0.6)) < 0.1  # falls back to the internal key


def test_gate_lookahead_change_no_click():
    # lookahead 0 -> 5 ms while a 1.1 kHz tone passes: the delay tap moves under a crossfade, never a jump
    sr = 48000
    x = signals.sine(1100, sr, 1.5, amp_db=-12.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=BASE, input=x, automation=[{"param": "lookahead_ms", "t_s": 0.5, "value": 5.0}])
    assert rep["nan_count"] == 0
    assert rep["latency_samples"] == 240
    seg = slice(int(0.4 * sr), int(1.4 * sr))
    assert np.max(np.abs(np.diff(y[seg, 0]))) < 1.5 * np.max(np.abs(np.diff(x[seg, 0])))


def test_gate_external_key_opens_quiet_main():
    sr = 48000
    main = signals.sine(1000, sr, 1.0, amp_db=-50.0, channels=1)  # 20 dB below the threshold on its own
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ext_key=1), input=main, layout={"in": 1, "out": 1, "sidechain": 1},
                         sidechain_db=30.0)
    assert abs(measure.level_at_db(y[:, 0], sr, 1000, start_s=0.6) - -50.0) < 0.1  # key -20 dB opens the gate


def test_gate_listen_sc():
    sr = 48000
    x = signals.sine(100, sr, 1.0, amp_db=-10.0, channels=1)
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, listen_sc=1, sc_hp_enabled=1, sc_hp_hz=1000.0), input=x)
    assert measure.level_at_db(y[:, 0], sr, 100, start_s=0.6) < -40.0  # we hear the filtered key, not the gated audio


def test_gate_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=0.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE, tol_dbfs=-60.0, settle_s=0.6))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))


def test_gate_lookahead_ramp_no_click():
    # M4 review finding (shared tap-crossfade pattern): a lookahead change landing inside the 5 ms tap crossfade
    # must not drop the half-faded tap
    sr = 48000
    x = signals.sine(1100.0, sr, 1.5, amp_db=-12.0, channels=1)
    for auto in ([{"param": "lookahead_ms", "t_s": 0.5 + 0.0014 * k, "value": 0.5 * k} for k in range(1, 11)],
                 [{"param": "lookahead_ms", "t_s": 0.5 + 0.002 * k, "value": 5.0 * (k % 2)} for k in range(20)]):
        y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, threshold_db=-80.0), input=x, automation=auto)
        assert rep["nan_count"] == 0
        seg = slice(int(0.4 * sr), int(1.4 * sr))
        assert np.max(np.abs(np.diff(y[seg, 0]))) < 1.5 * np.max(np.abs(np.diff(x[seg, 0])))
