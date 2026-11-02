import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_plugin
from tests.plugins import base_checks

PLUGIN = "saturation"
CHARACTERS = ["Tape", "Tube", "Transistor", "Clip"]
BASE = {"drive_db": 6.0, "character": 0, "bias": 0.0, "hf_rolloff_hz": 20000.0, "mix_pct": 100.0, "output_db": 0.0}
# reference drive per character (ADR-0006): the drive at which a 1 kHz tone at -12 dBFS gives 1 % THD; the plugin
# computes it (SaturationProcessor::referenceDriveDb); mirrored here from tests/cpp/test_saturation.cpp's printout
REFERENCE_DRIVE_DB = {"Tape": 0.0, "Tube": 2.96, "Transistor": 2.96, "Clip": 8.79}


def spectrum_db(y, sr):
    n = len(y)
    w = np.blackman(n)
    s = np.fft.rfft(y * w) / (np.sum(w) / 2)
    return np.fft.rfftfreq(n, 1 / sr), 20 * np.log10(np.abs(s) + 1e-30)


def worst_alias_db(sr, params, tone_hz=19000.0, level_db=-12.0):
    x = signals.sine(tone_hz, sr, 1.0, amp_db=level_db, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    seg = y[int(0.3 * sr):int(0.3 * sr) + 32768, 0]
    f, mag = spectrum_db(seg, sr)
    keep = (f < tone_hz - 200.0) & (f > 20.0)
    return float(np.max(mag[keep]))


def harmonics_db(sr, params, n=16, level_db=-12.0):
    x = signals.sine(1000.0, sr, 1.0, amp_db=level_db, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    seg = y[int(0.3 * sr):, 0]
    return np.array([measure.level_at_db(seg, sr, 1000.0 * k) for k in range(1, n + 1)])


def thd_pct(h):
    lin = 10 ** (h / 20)
    return 100.0 * np.sqrt(np.sum(lin[1:] ** 2)) / lin[0]


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("character", CHARACTERS)
def test_sat_aliasing_19k(sr, character, record_property):
    ref = REFERENCE_DRIVE_DB[character]
    worst = worst_alias_db(sr, dict(BASE, character=CHARACTERS.index(character), drive_db=ref))
    record_property("alias_db_reference_drive", worst)
    for drive in (12.0, 36.0):  # recorded, not gated (ADR-0006)
        record_property(f"alias_db_drive_{int(drive)}", worst_alias_db(sr, dict(BASE, character=CHARACTERS.index(character), drive_db=drive)))
    assert worst < -80.0


@pytest.mark.parametrize("character", CHARACTERS)
def test_sat_reference_drive_thd(character, record_property):
    h = harmonics_db(48000, dict(BASE, character=CHARACTERS.index(character), drive_db=REFERENCE_DRIVE_DB[character]))
    thd = thd_pct(h)
    record_property("thd_pct_at_reference_drive", float(thd))
    assert abs(thd - 1.0) < 0.25


@pytest.mark.parametrize("character", CHARACTERS)
def test_sat_harmonic_balance(character, record_property):
    sr = 48000
    ratios = []
    for bias in (-1.0, -0.5, 0.0, 0.5, 1.0):
        h = harmonics_db(sr, dict(BASE, character=CHARACTERS.index(character), drive_db=12.0, bias=bias))
        lin = 10 ** (h / 20)
        even, odd = np.sqrt(np.sum(lin[1::2] ** 2)), np.sqrt(np.sum(lin[2::2] ** 2))
        ratios.append(20 * np.log10(even / odd + 1e-30))
    record_property("even_odd_db_by_bias", [round(float(r), 1) for r in ratios])
    assert ratios[2] < -40.0 or character == "Tape"  # bias 0: no even harmonics (Tape's emphasis/head bump are linear: also none)
    assert ratios[3] > ratios[2] + 20.0 and ratios[1] > ratios[2] + 20.0  # any bias brings even harmonics in
    assert ratios[4] > ratios[3] and ratios[0] > ratios[1]  # more bias, more even content


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_sat_latency_reported(sr):
    x = signals.noise(sr, 0.5, seed=3, amp_db=-30.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, drive_db=0.0), input=x)
    lat = rep["latency_samples"]
    assert lat > 0
    xc = np.correlate(y[:, 0], x[:, 0], mode="full")
    assert int(np.argmax(xc)) - (len(x) - 1) == lat


def test_sat_null_mix_zero():
    sr = 48000
    x = signals.noise(sr, 1.0, seed=4, amp_db=-12.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, mix_pct=0.0, drive_db=36.0), input=x)
    lat = rep["latency_samples"]
    assert measure.residual_dbfs(y[lat:, 0], x[:len(x) - lat, 0]) < -90.0


@pytest.mark.parametrize("character", CHARACTERS)
def test_sat_unity_small_signal(character):
    # drive adds colour, not level: a -60 dBFS tone (0.06 into the shaper at 36 dB drive: still small-signal) keeps
    # its level within 0.5 dB from drive 0 to 36
    sr = 48000
    levels = []
    for drive in (0.0, 12.0, 24.0, 36.0):
        x = signals.sine(1000.0, sr, 0.5, amp_db=-60.0, channels=1)
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, character=CHARACTERS.index(character), drive_db=drive), input=x)
        levels.append(measure.level_at_db(y[:, 0], sr, 1000.0, start_s=0.2))
    assert max(levels) - min(levels) < 0.5
    assert abs(levels[0] - -60.0) < 0.1


@pytest.mark.parametrize("character", CHARACTERS)
def test_sat_bounded_and_dc(character):
    # Review Focus 1: full-scale square-ish input at max drive and any bias: bounded, no DC
    sr = 48000
    t = np.arange(int(0.5 * sr)) / sr
    x = (0.99 * np.sign(np.sin(2 * np.pi * 80.0 * t)) * (1.0 - 0.3 * np.sin(2 * np.pi * 7.0 * t))).astype(np.float32)[:, None]
    for bias in (-1.0, 0.0, 1.0):
        y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, character=CHARACTERS.index(character), drive_db=36.0, bias=bias), input=x)
        assert rep["nan_count"] == 0
        assert rep["peak_db"] < 6.0
        dc = np.mean(y[int(0.1 * sr):, 0])
        assert 20 * np.log10(abs(dc) + 1e-12) < -60.0


def test_sat_hf_rolloff():
    sr = 48000
    x = signals.sine(10000.0, sr, 0.5, amp_db=-30.0, channels=1)
    a, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, drive_db=0.0, hf_rolloff_hz=20000.0), input=x)
    b, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, drive_db=0.0, hf_rolloff_hz=10000.0), input=x)
    assert abs((measure.level_at_db(b[:, 0], sr, 10000.0, start_s=0.2) - measure.level_at_db(a[:, 0], sr, 10000.0, start_s=0.2)) - -3.01) < 0.3


def test_sat_extremes_and_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=12.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE, tol_dbfs=-60.0, settle_s=0.1))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))
