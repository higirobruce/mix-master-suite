import numpy as np
import pytest
from scipy import signal

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES, assert_response, measure_band
from tests.harness.measure import residual_dbfs
from tests.harness.render import render_module
from tests.core.test_biquad import response


def lr_reference(order, sr, fc, f):
    """Cascaded Butterworth (bilinear) low/high-pass of half the LR order, squared: the LR response."""
    n = order // 2
    bl, al = signal.butter(n, fc, btype="low", fs=sr)
    bh, ah = signal.butter(n, fc, btype="high", fs=sr)
    w = 2 * np.pi * np.asarray(f) / sr
    _, hl = signal.freqz(bl, al, worN=w)
    _, hh = signal.freqz(bh, ah, worN=w)
    lo, hi = hl * hl, hh * hh
    if n % 2 == 1:  # LR2/LR6: invert the high band so the sum is all-pass
        hi = -hi
    return lo, hi


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("order", [2, 4, 8])
@pytest.mark.parametrize("fc", [120.0, 2000.0])
def test_lr_bands_match_butterworth_squared(sr, order, fc):
    x = signals.impulse(sr, 65536 / sr, channels=1)
    lo, hi = measure_band(sr)
    f = np.logspace(np.log10(lo), np.log10(hi), 50)
    ref_lo, ref_hi = lr_reference(order, sr, fc, f)
    for out, ref in ((0, ref_lo), (1, ref_hi)):
        y, rep = render_module("lr_crossover", sr=sr, params={"freq_hz": fc, "order": order, "output": out}, input=x)
        assert rep["nan_count"] == 0
        mag, ph = response(y[:, 0].astype(np.float64), sr, f)
        rmag, rph = 20 * np.log10(np.abs(ref) + 1e-300), np.degrees(np.angle(ref))
        keep = rmag > -100
        assert_response(f[keep], mag[keep], ph[keep], rmag[keep], rph[keep], (lo, hi), label=f"LR{order} out{out} fc={fc} @ {sr}")


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("order", [2, 4, 8])
def test_lr_sum_is_allpass(sr, order):
    """low + high (the module's 'sum' output) is an all-pass: flat within 0.01 dB across the band."""
    x = signals.impulse(sr, 65536 / sr, channels=1)
    y, rep = render_module("lr_crossover", sr=sr, params={"freq_hz": 800.0, "order": order, "output": 2}, input=x)
    assert rep["nan_count"] == 0
    lo, hi = measure_band(sr)
    f = np.logspace(np.log10(lo), np.log10(hi), 80)
    mag, _ = response(y[:, 0].astype(np.float64), sr, f)
    assert np.max(np.abs(mag)) < 0.01
