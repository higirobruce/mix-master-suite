import numpy as np
import pytest
from scipy import signal

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("taps", [63, 255])
def test_fir_module_matches_scipy_kaiser_design(sr, taps):
    """The module's impulse response equals scipy's Kaiser windowed-sinc for the same taps/cutoff/beta."""
    x = signals.impulse(sr, (taps + 64) / sr, channels=1)
    y, rep = render_module("fir", sr=sr, params={"taps": taps, "cutoff_norm": 0.2, "window": 3, "beta": 8.6}, input=x)
    assert rep["nan_count"] == 0 and rep["latency_samples"] == (taps - 1) // 2
    ref = signal.firwin(taps, 0.4, window=("kaiser", 8.6))  # firwin cutoff is relative to Nyquist
    np.testing.assert_allclose(y[:taps, 0], ref, atol=2e-7)
