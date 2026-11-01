import pytest

from tests.harness import signals
from tests.harness.render import render_module
from tests.core.test_oversampler import worst_alias_dbfs


def alias(sr, module, params):
    x = signals.sine(19000.0, sr, 1.0, amp_db=0.0, channels=1)
    y, rep = render_module(module, sr=sr, params=params, input=x)
    assert rep["nan_count"] == 0
    return worst_alias_dbfs(y, sr, 19000.0)


@pytest.mark.parametrize("sr", [44100, 48000, 96000])
def test_tube_alias_gate_at_4x(sr, record_property):
    worst = alias(sr, "tube", {"drive": 1.0, "bias": 0.5, "oversample": 4})
    record_property("worst_alias_dbfs", worst)
    assert worst < -80


@pytest.mark.parametrize("sr", [44100, 48000, 96000])
def test_tape_alias_gate_at_8x_and_4x_recorded(sr, record_property):
    """The hysteresis model's sharp dynamics alias more than a static shaper: at 4x it reads -68/-80/-90 dBFS at
    44.1/48/96 k (recorded for ADR-0006); the gate is met at 8x."""
    record_property("alias_4x_dbfs", alias(sr, "tape", {"drive": 0.5, "saturation": 0.5, "width": 0.5, "oversample": 4}))
    worst8 = alias(sr, "tape", {"drive": 0.5, "saturation": 0.5, "width": 0.5, "oversample": 8})
    record_property("alias_8x_dbfs", worst8)
    assert worst8 < -80
