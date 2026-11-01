import numpy as np
import pytest

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES, assert_response, measure_band
from tests.harness.render import render_module
from tests.core.test_biquad import response


def analytic(kind, sr, fc, q, f):
    """Bilinear one-pole / all-pass references."""
    w = 2 * np.pi * np.asarray(f) / sr
    z = np.exp(-1j * w)
    g = np.tan(np.pi * fc / sr)
    if kind == "onepole_hp":
        G = g / (1 + g)
        # TPT one-pole: lp = G (x - s) + s ... closed form H_lp = G (1 + z^-1) / (1 - (1 - 2G) z^-1)
        h_lp = G * (1 + z) / (1 - (1 - 2 * G) * z)
        h = 1 - h_lp
    elif kind == "onepole_lp":
        G = g / (1 + g)
        h = G * (1 + z) / (1 - (1 - 2 * G) * z)
    elif kind == "allpass1":
        c = (g - 1) / (g + 1)
        h = (c + z) / (1 + c * z)
    else:  # allpass2 (RBJ)
        from tests.harness.designs import rbj
        b, a = rbj("allpass", sr, fc, 0.0, q)
        from tests.harness.designs import freqz_db_deg
        return freqz_db_deg(b, a, f, sr)
    return 20 * np.log10(np.abs(h) + 1e-300), np.degrees(np.angle(h))


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("module,kind", [("onepole", "onepole_hp"), ("onepole", "onepole_lp"), ("allpass1", "allpass1"), ("allpass2", "allpass2")])
@pytest.mark.parametrize("fc", [50.0, 1000.0, 8000.0])
def test_first_second_order_sections_match_analytic(sr, module, kind, fc, record_property):
    params = {"freq_hz": fc, "q": 0.8}
    if module == "onepole":
        params["kind"] = 0 if kind == "onepole_hp" else 1
    x = signals.impulse(sr, 65536 / sr, channels=1)
    y, rep = render_module(module, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    lo, hi = measure_band(sr)
    f = np.logspace(np.log10(lo), np.log10(hi), 40)
    mag, ph = response(y[:, 0].astype(np.float64), sr, f)
    rmag, rph = analytic(kind, sr, fc, 0.8, f)
    keep = rmag > -100  # skip the deep stopband (float noise floor)
    d = assert_response(f[keep], mag[keep], ph[keep], rmag[keep], rph[keep], (lo, hi), label=f"{kind} fc={fc} @ {sr}")
    record_property("max_dev_db", d[0])
    if kind.startswith("allpass"):
        assert np.max(np.abs(mag)) < 0.01
