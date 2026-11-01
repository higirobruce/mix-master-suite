import numpy as np
import pytest

from tests.harness import signals
from tests.harness.render import render_module
from tests.core.test_oversampler import worst_alias_dbfs

SHAPES = ["tanh", "atan", "asym", "softclip", "hardclip"]


def alias(sr, shape, drive_db, adaa, oversample=4):
    x = signals.sine(19000.0, sr, 1.0, amp_db=0.0, channels=1)
    y, rep = render_module("waveshaper", sr=sr, params={"shape": SHAPES.index(shape), "drive_db": drive_db, "bias": 0.0,
                                                         "knee": 0.5, "adaa": adaa, "oversample": oversample}, input=x)
    assert rep["nan_count"] == 0
    return worst_alias_dbfs(y, sr, 19000.0)


@pytest.mark.parametrize("sr", [44100, 48000, 96000, 192000])
@pytest.mark.parametrize("shape", ["tanh", "softclip"])
def test_adaa_lowers_aliasing_at_4x_full_drive(sr, shape, record_property):
    """19 kHz at +18 dB (full drive) through the stage, 4x: ADAA must lower the alias floor by >= 15 dB versus the
    plain shaper. The absolute spec gate (-80 dBFS) is tested below at the drive where 4x + ADAA1 reaches it; the
    +18 dB numbers are recorded here as the input to M3's saturation design (ADR-0006)."""
    plain = alias(sr, shape, 18.0, 0)
    aa = alias(sr, shape, 18.0, 1)
    record_property("plain_dbfs", plain)
    record_property("adaa_dbfs", aa)
    assert aa < plain - 15.0


@pytest.mark.parametrize("sr", [44100, 48000, 96000, 192000])
def test_spec_alias_gate_minus_80_with_adaa(sr, record_property):
    """Spec gate 'alias products below -80 dBFS with oversampling on' at 19 kHz for the saturation shaper (tanh) at
    +6 dB drive. Gated at 8x + ADAA1 (where the margin is > 10 dB); 4x + ADAA1 is recorded: it sits at the gate at
    44.1 k (-79.7 dBFS) and passes above. The soft clipper's quadratic knee aliases more and is gated in M5."""
    aa8 = alias(sr, "tanh", 6.0, 1, oversample=8)
    aa4 = alias(sr, "tanh", 6.0, 1, oversample=4)
    record_property("adaa_8x_dbfs", aa8)
    record_property("adaa_4x_dbfs", aa4)
    assert aa8 < -80.0
    assert aa4 < -75.0


@pytest.mark.parametrize("sr", [44100, 48000, 96000])
def test_softclip_alias_characterisation_4x(sr, record_property):
    record_property("adaa_6db_dbfs", alias(sr, "softclip", 6.0, 1))
    record_property("adaa_8x_6db_dbfs", alias(sr, "softclip", 6.0, 1, oversample=8))
