import numpy as np
import pytest

from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("mode", [0, 1])
def test_smoother_ramp_has_no_zipper(sr, mode):
    """0 -> 1 step at t=0.1 s with a 20 ms ramp: no sample-to-sample jump above the linear slope, and the ramp
    has no energy above 2 kHz worth mentioning (zipper noise would)."""
    y, rep = render_module("smoother", sr=sr, block=64, params={"ramp_ms": 20, "mode": mode},
                           input=np.zeros((int(0.3 * sr), 1), dtype=np.float32),
                           automation=[{"param": "target", "t_s": 0.1, "value": 1.0}])
    assert rep["nan_count"] == 0
    y = y[:, 0].astype(np.float64)
    step = np.max(np.abs(np.diff(y)))
    assert step <= (1.0 / (0.02 * sr)) * (1.01 if mode == 0 else 7.3)  # exponential's first step is ln(1000) ~ 6.9x the linear slope
    assert abs(y[-1] - 1.0) < 1e-5
    ramp = y[int(0.1 * sr) - 10:int(0.1 * sr) + int(0.06 * sr)]
    spec = np.abs(np.fft.rfft(ramp * np.hanning(len(ramp))))
    f = np.fft.rfftfreq(len(ramp), 1 / sr)
    hf = spec[f > 2000].max() / spec.max()
    assert 20 * np.log10(hf + 1e-30) < -80
