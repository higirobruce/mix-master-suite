import numpy as np
import pytest

from tests.harness.matrix import assert_response, measure_band


def test_measure_band_caps_at_20k_and_045fs():
    assert measure_band(48000) == (20.0, 20000.0)
    assert measure_band(44100) == (20.0, pytest.approx(19845.0))


def test_assert_response_passes_small_and_fails_large_deviation():
    f = np.logspace(1, 4.3, 50)
    ref = np.zeros_like(f)
    assert_response(f, ref + 0.05, ref, ref, ref, (20, 20000))
    with pytest.raises(AssertionError):
        assert_response(f, ref + 0.2, ref, ref, ref, (20, 20000))
    with pytest.raises(AssertionError):
        assert_response(f, ref, ref + 3.0, ref, ref, (20, 20000))
    # phase wrap: 179 vs -179 is a 2 degree difference, not 358
    assert_response(f, ref, ref + 179.0, ref, ref - 179.0, (20, 20000))
