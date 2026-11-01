import numpy as np
import pytest

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES, measure_band
from tests.harness.measure import residual_dbfs
from tests.harness.render import render_module
from tests.core.test_biquad import response

MODES = ["lowpass", "highpass", "bandpass", "notch", "peak", "lowshelf", "highshelf", "allpass"]


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("mode", MODES)
def test_svf_renders_finite_and_unity_modes_null(sr, mode):
    x = signals.noise(sr, 0.5, seed=4, channels=1)
    gain = 0.0 if mode in ("peak", "lowshelf", "highshelf") else 6.0
    y, rep = render_module("svf", sr=sr, params={"mode": MODES.index(mode), "freq_hz": 1000, "q": 0.7, "gain_db": gain}, input=x)
    assert rep["nan_count"] == 0
    if mode in ("peak", "lowshelf", "highshelf"):
        assert residual_dbfs(y, x) < -120


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_svf_lowpass_matches_rbj_lowpass_shape(sr):
    """TPT SVF low-pass and the RBJ bilinear low-pass are the same transfer function (both bilinear, same Q)."""
    from tests.harness import designs
    from tests.harness.matrix import assert_response
    f0, q = 2000.0, 1.2
    x = signals.impulse(sr, 65536 / sr, channels=1)
    y, _ = render_module("svf", sr=sr, params={"mode": 0, "freq_hz": f0, "q": q, "gain_db": 0}, input=x)
    lo, hi = measure_band(sr)
    f = np.logspace(np.log10(lo), np.log10(min(hi, 10 * f0)), 40)
    mag, ph = response(y[:, 0].astype(np.float64), sr, f)
    b, a = designs.rbj("lowpass", sr, f0, 0.0, q)
    rmag, rph = designs.freqz_db_deg(b, a, f, sr)
    assert_response(f, mag, ph, rmag, rph, (lo, 10 * f0), label=f"svf lowpass @ {sr}")
