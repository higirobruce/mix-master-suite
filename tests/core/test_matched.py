import numpy as np
import pytest

from tests.harness import designs, signals
from tests.harness.matrix import SAMPLE_RATES, assert_response, measure_band
from tests.harness.render import render_module
from tests.core.test_biquad import response

TYPES = ["peak", "lowshelf", "highshelf", "lowpass", "highpass"]


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("kind", TYPES)
@pytest.mark.parametrize("f0", [200.0, 2000.0, 8000.0])
@pytest.mark.parametrize("gain", [-9.0, 6.0])
def test_matched_module_equals_python_mirror(sr, kind, f0, gain, record_property):
    q = 1.5 if kind == "peak" else 0.7
    x = signals.impulse(sr, 65536 / sr, channels=1)
    y, rep = render_module("biquad", sr=sr, params={"type": TYPES.index(kind), "freq_hz": f0, "gain_db": gain, "q": q,
                                                     "design": 1}, input=x)
    assert rep["nan_count"] == 0
    lo, hi = measure_band(sr)
    f = np.logspace(np.log10(lo), np.log10(hi), 50)
    mag, ph = response(y[:, 0].astype(np.float64), sr, f)
    b, a = designs.matched(kind, sr, f0, gain, q)
    rmag, rph = designs.freqz_db_deg(b, a, f, sr)
    d_mag, d_deg = assert_response(f, mag, ph, rmag, rph, (lo, hi), label=f"matched {kind} {f0} Hz {gain} dB @ {sr}")
    record_property("max_dev_db", d_mag)


@pytest.mark.parametrize("sr", [44100, 48000])
@pytest.mark.parametrize("kind", ["peak", "lowshelf", "highshelf"])
def test_matched_vs_analog_report(sr, kind, record_property):
    """Informational for the spec question: worst |matched - analog| and |rbj - analog| up to 20 kHz."""
    f0, gain, q = 10000.0, 6.0, 1.0
    f = np.logspace(2, np.log10(min(20000, 0.49 * sr)), 200)
    analog = 10 * np.log10(designs.analog_prototype(kind, f0, gain, q, f))
    bm, am = designs.matched(kind, sr, f0, gain, q)
    br, ar = designs.rbj(kind, sr, f0, gain, q)
    dm = np.max(np.abs(designs.freqz_db_deg(bm, am, f, sr)[0] - analog))
    dr = np.max(np.abs(designs.freqz_db_deg(br, ar, f, sr)[0] - analog))
    record_property("matched_vs_analog_db", float(dm))
    record_property("rbj_vs_analog_db", float(dr))
    assert dm < dr
