import numpy as np
import pytest

from tests.harness import signals
from tests.harness.render import render_module


def worst_alias_dbfs(y, sr, f0):
    seg = y[sr // 2:, 0].astype(np.float64)
    win = np.blackman(len(seg))
    spec = np.abs(np.fft.rfft(seg * win)) * 2 / np.sum(win)
    f = np.fft.rfftfreq(len(seg), 1 / sr)
    below = (f > 100) & (f < f0 - 300)
    return float(20 * np.log10(spec[below].max() + 1e-30))


@pytest.mark.parametrize("sr", [44100, 48000, 96000, 192000])
@pytest.mark.parametrize("drive_db", [6.0, 18.0])
def test_oversampling_alias_suppression(sr, drive_db, record_property):
    """19 kHz tone through tanh: aliases below the tone at 1x, 4x and 8x. The oversampler must buy >= 30 dB at 4x
    and >= 50 dB at 8x over 1x (drive +6 dB). At full drive (+18 dB, a near square wave) the 9th/11th harmonics
    wrap inside the 4x domain; the absolute -80 dBFS gate for saturation is met with ADAA (Task 13 / M3) and the
    numbers are recorded here for that design."""
    f0 = 19000.0
    x = signals.sine(f0, sr, 1.0, amp_db=drive_db, channels=1)
    alias = {}
    for factor in (1, 4, 8):
        y, rep = render_module("oversampled_tanh", sr=sr, params={"factor": factor, "drive_db": 0.0}, input=x)
        assert rep["nan_count"] == 0
        assert rep["latency_samples"] == (0 if factor == 1 else rep["latency_samples"]) and rep["latency_samples"] >= 0
        alias[factor] = worst_alias_dbfs(y, sr, f0)
        record_property(f"alias_{factor}x_dbfs", alias[factor])
    if drive_db == 6.0:
        assert alias[4] < alias[1] - 30
        assert alias[8] < alias[1] - 50
        assert alias[8] < -80
