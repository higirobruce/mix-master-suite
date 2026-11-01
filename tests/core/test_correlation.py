import numpy as np
import pytest

from tests.harness import signals
from tests.harness.render import render_module


@pytest.mark.parametrize("sr", [44100, 48000, 96000, 192000])
def test_correlation_module_outputs_running_correlation(sr):
    x = signals.noise(sr, 1.0, seed=9, channels=2)
    x[:, 1] = x[:, 0]
    y, rep = render_module("correlation", sr=sr, params={}, input=x)
    assert rep["nan_count"] == 0
    assert abs(y[-1, 0] - 1.0) < 0.001
    x[:, 1] = -x[:, 0]
    y, _ = render_module("correlation", sr=sr, params={}, input=x)
    assert abs(y[-1, 0] + 1.0) < 0.001
