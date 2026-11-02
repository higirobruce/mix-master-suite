"""Reverb core (ADR-0008): Schroeder backward-integrated RT60 of the FDN and the Dattorro plate at zero damping must
land within 10 % of the label at 0.5 / 2 / 8 s and four sample rates; damping shortens the high band; energy decays."""
import numpy as np
import pytest

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module


def schroeder_t60(y, sr, start_s=0.02):
    """T60 from the energy decay curve, fitted between -5 and -35 dB (ISO 3382 T30 style, extrapolated to 60)."""
    e = np.cumsum(y[::-1] ** 2)[::-1]
    edc = 10 * np.log10(e / e[int(start_s * sr)] + 1e-30)
    t = np.arange(len(y)) / sr
    i0 = np.argmax(edc <= -5.0)
    i1 = np.argmax(edc <= -35.0)
    assert i1 > i0 > 0, "decay never reached -35 dB"
    slope = (edc[i1] - edc[i0]) / (t[i1] - t[i0])
    return -60.0 / slope


def impulse_response(module, sr, params, seconds):
    x = signals.impulse(sr, seconds, channels=2)
    y, rep = render_module(module, sr=sr, params=params, input=x, channels=2)
    assert rep["nan_count"] == 0
    return y


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("rt60", [0.5, 2.0, 8.0])
@pytest.mark.parametrize("module,extra", [("fdn", {"algorithm": 0}), ("fdn", {"algorithm": 1}), ("fdn", {"algorithm": 2}), ("plate", {})])
def test_reverb_rt60_within_10_percent(sr, rt60, module, extra, record_property):
    y = impulse_response(module, sr, dict({"rt60_s": rt60, "size_pct": 60.0, "damping_pct": 0.0, "diffusion_pct": 70.0}, **extra), min(rt60 * 1.6 + 0.5, 14.0))
    t60 = schroeder_t60(y[:, 0] + y[:, 1], sr)
    record_property("t60_measured", float(t60))
    assert abs(t60 - rt60) / rt60 < 0.10


@pytest.mark.parametrize("module", ["fdn", "plate"])
def test_reverb_damping_shortens_high_band(module, record_property):
    sr = 48000
    from scipy.signal import butter, sosfilt
    y = impulse_response(module, sr, {"rt60_s": 2.0, "size_pct": 60.0, "damping_pct": 100.0, "diffusion_pct": 70.0, "algorithm": 1}, 4.0)
    m = y[:, 0] + y[:, 1]
    lo = sosfilt(butter(4, [350, 700], btype="band", fs=sr, output="sos"), m)
    hi = sosfilt(butter(4, [5600, 11200], btype="band", fs=sr, output="sos"), m)
    t_lo, t_hi = schroeder_t60(lo, sr), schroeder_t60(hi, sr)
    record_property("t60_500hz", float(t_lo))
    record_property("t60_8khz", float(t_hi))
    assert t_hi < 0.5 * t_lo


@pytest.mark.parametrize("module", ["fdn", "plate"])
def test_reverb_energy_decreases(module):
    sr = 48000
    y = impulse_response(module, sr, {"rt60_s": 20.0, "size_pct": 100.0, "damping_pct": 0.0, "diffusion_pct": 100.0, "algorithm": 1}, 12.0)
    m = y[:, 0] + y[:, 1]
    windows = [np.sum(m[i:i + sr] ** 2) for i in range(sr, len(m) - sr, sr)]
    assert all(b < a for a, b in zip(windows, windows[1:]))
