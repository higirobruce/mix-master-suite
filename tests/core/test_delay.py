import numpy as np
import pytest

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("delay_ms", [3.7, 25.0, 250.0])
@pytest.mark.parametrize("interp", [0, 1])
def test_echo_time_within_0p05_ms(sr, delay_ms, interp, record_property):
    x = signals.impulse(sr, 0.6, channels=1)
    y, rep = render_module("delay", sr=sr, params={"delay_ms": delay_ms, "interp": interp, "feedback": 0.0, "slew_ms": 0.0}, input=x)
    assert rep["nan_count"] == 0
    # cross-correlation peak with sub-sample refinement (parabolic) against the impulse
    idx = int(np.argmax(np.abs(y[:, 0])))
    a, b, c = (abs(y[idx - 1, 0]), abs(y[idx, 0]), abs(y[idx + 1, 0]))
    frac = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
    measured_ms = 1000.0 * (idx + frac) / sr
    record_property("delay_error_ms", float(measured_ms - delay_ms))
    assert abs(measured_ms - delay_ms) < 0.05


def test_feedback_taps_decay_geometrically():
    sr = 48000
    x = signals.impulse(sr, 1.0, channels=1)
    y, _ = render_module("delay", sr=sr, params={"delay_ms": 100.0, "interp": 0, "feedback": 0.5, "slew_ms": 0.0}, input=x)
    taps = [np.max(np.abs(y[int((k * 0.1 - 0.001) * sr):int((k * 0.1 + 0.001) * sr), 0])) for k in range(1, 6)]
    for k, amp in enumerate(taps, start=1):
        assert abs(20 * np.log10(amp) - 20 * np.log10(0.5 ** (k - 1))) < 0.2  # wet-only: first echo at unity
