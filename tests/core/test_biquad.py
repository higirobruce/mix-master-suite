import numpy as np
import pytest

from tests.harness import designs, signals
from tests.harness.matrix import SAMPLE_RATES, assert_response, measure_band
from tests.harness.render import render_module

TYPES = ["peak", "lowshelf", "highshelf", "lowpass", "highpass", "notch", "allpass"]


def response(y, sr, f):
    """Magnitude/phase of an impulse response at frequencies f (naive DFT, exact)."""
    n = np.arange(len(y))
    h = np.array([np.sum(y * np.exp(-2j * np.pi * fi * n / sr)) for fi in f])
    return 20 * np.log10(np.abs(h) + 1e-300), np.degrees(np.angle(h))


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("kind", TYPES)
@pytest.mark.parametrize("f0", [200.0, 1000.0, 5000.0])
@pytest.mark.parametrize("gain", [-12.0, 6.0])
def test_biquad_response_matches_design(sr, kind, f0, gain, record_property):
    q = 2.0 if kind in ("peak", "notch") else 0.7
    x = signals.impulse(sr, 65536 / sr, channels=1)
    y, rep = render_module("biquad", sr=sr, params={"type": TYPES.index(kind), "freq_hz": f0, "gain_db": gain, "q": q},
                           input=x)
    assert rep["nan_count"] == 0
    lo, hi = measure_band(sr)
    f = np.logspace(np.log10(lo), np.log10(hi), 60)
    if kind == "notch":
        f = f[np.abs(f - f0) / f0 > 0.05]
    mag, ph = response(y[:, 0].astype(np.float64), sr, f)
    b, a = designs.rbj(kind, sr, f0, gain, q)
    rmag, rph = designs.freqz_db_deg(b, a, f, sr)
    d_mag, d_deg = assert_response(f, mag, ph, rmag, rph, (lo, hi), label=f"{kind} {f0} Hz {gain} dB @ {sr}")
    record_property("max_dev_db", d_mag)
    record_property("max_dev_deg", d_deg)


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_biquad_unity_nulls(sr):
    x = signals.noise(sr, 1.0, seed=3, channels=1)
    y, _ = render_module("biquad", sr=sr, params={"type": 0, "freq_hz": 1000, "gain_db": 0, "q": 1}, input=x)
    from tests.harness.measure import residual_dbfs
    assert residual_dbfs(y, x) < -120
