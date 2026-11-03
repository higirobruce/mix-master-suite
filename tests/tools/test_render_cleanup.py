"""The render harness leaves nothing behind: every render's temp directory (input, output, job, report) is removed
once the result is in memory. A session of the measurement suite made 15 000 of them (36 GB) before this test."""
from __future__ import annotations

import tempfile
from pathlib import Path

from tests.harness import signals
from tests.harness.render import render_module


def test_render_removes_its_temp_directory():
    before = set(Path(tempfile.gettempdir()).glob("mm_render_*"))
    x = signals.noise(48000, 0.1, seed=1, amp_db=-12.0)
    y, rep = render_module("oversampled_tanh", sr=48000, params={"factor": 4, "drive_db": -80.0}, input=x, channels=2)
    assert y.shape[0] == x.shape[0]
    assert rep["nan_count"] == 0
    assert set(Path(tempfile.gettempdir()).glob("mm_render_*")) - before == set()
