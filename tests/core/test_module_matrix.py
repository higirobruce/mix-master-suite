"""Every registered core module at every sample rate: silence in -> silence out, noise in -> finite, and for the
linear modules a block-size schedule renders identically to a fixed block size."""
import json
import subprocess

import numpy as np
import pytest

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.measure import residual_dbfs
from tests.harness.render import mm_render_path, render_module

EXPECTED = ["allpass1", "allpass2", "biquad", "compressor_core", "correlation", "delay", "envelope", "fdn", "fir", "identity",
            "lr_crossover", "onepole", "oversampled_tanh", "plate", "smoother", "svf", "tape", "tube", "waveshaper"]
LINEAR = ["allpass1", "allpass2", "biquad", "delay", "fir", "identity", "lr_crossover", "onepole", "svf"]
SILENT_OUT = [m for m in EXPECTED if m not in ("smoother", "envelope", "correlation")]  # these emit control signals


def test_registry_lists_every_module():
    out = subprocess.run([str(mm_render_path()), "--list-modules"], capture_output=True, text=True, check=True).stdout
    assert sorted(json.loads(out)) == sorted(EXPECTED)


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("module", EXPECTED)
def test_silence_and_noise(sr, module):
    silence = np.zeros((int(0.2 * sr), 2), dtype=np.float32)
    y, rep = render_module(module, sr=sr, params={}, input=silence)
    assert rep["nan_count"] == 0
    if module in SILENT_OUT:
        assert np.max(np.abs(y)) < 1e-6
    noise = signals.noise(sr, 0.5, seed=21, channels=2)
    y, rep = render_module(module, sr=sr, params={}, input=noise)
    assert rep["nan_count"] == 0
    assert np.all(np.isfinite(y))


@pytest.mark.parametrize("sr", [48000, 192000])
@pytest.mark.parametrize("module", LINEAR)
def test_linear_modules_are_block_size_independent(sr, module):
    x = signals.noise(sr, 1.0, seed=22, channels=2)
    ref, _ = render_module(module, sr=sr, params={}, input=x, block=64)
    sched = [{"t_s": t, "block_size": b} for t, b in zip(np.linspace(0, 0.9, 5), [16, 4096, 1, 500, 63])]
    var, rep = render_module(module, sr=sr, params={}, input=x, block=64, schedule=sched)
    assert rep["nan_count"] == 0
    assert residual_dbfs(var, ref) < -120
