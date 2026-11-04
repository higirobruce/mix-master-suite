"""The spec's typical mastering chain in series (LP-EQ -> Multiband -> Bus comp -> M/S -> Soft clipper -> Limiter)
on a synthetic mix: the output lands at the loudness target the test computes, never exceeds the ceiling, and the
six plugins' summed cost is recorded against the sum of their budgets (M5 Task 9)."""
from __future__ import annotations

import os

import numpy as np
import pytest

from tests.harness import signals
from tests.harness.render import bench_plugin, render_plugin
from tests.plugins import base_checks

CHAIN = ["lp_eq", "multiband_comp", "bus_comp", "ms_processor", "soft_clipper", "limiter"]
TARGET_LUFS = -14.0
CEILING_DBTP = -1.0


def mix_fixture(sr: int, seconds: float = 8.0) -> np.ndarray:
    """Noise with a few tones, a slow 2 Hz amplitude pump and a stereo spread: dense enough to meter, dynamic
    enough to limit."""
    n = int(seconds * sr)
    t = np.arange(n) / sr
    rng = np.random.default_rng(23)
    pump = 0.6 + 0.4 * np.sin(2 * np.pi * 2.0 * t) ** 2
    l = 0.08 * rng.standard_normal(n) + 0.15 * np.sin(2 * np.pi * 110.0 * t) + 0.08 * np.sin(2 * np.pi * 1320.0 * t)
    r = 0.08 * rng.standard_normal(n) + 0.15 * np.sin(2 * np.pi * 110.0 * t) + 0.08 * np.sin(2 * np.pi * 1760.0 * t + 0.5)
    return (np.stack([l, r], axis=1) * pump[:, None]).astype(np.float32)


def run_chain(x: np.ndarray, sr: int, limiter_input_db: float):
    params = {
        "lp_eq": {"b2_enabled": 1, "b2_gain_db": 1.0, "b7_enabled": 1, "b7_gain_db": 1.5, "fir_length": 1},
        "multiband_comp": {"num_bands": 1, "b1_threshold_db": -20.0, "b1_ratio": 2.0, "b2_threshold_db": -20.0, "b2_ratio": 2.0,
                           "b3_threshold_db": -20.0, "b3_ratio": 2.0, "b4_threshold_db": -20.0, "b4_ratio": 2.0},
        "bus_comp": {"threshold_db": -10.0, "ratio": 1, "attack": 4, "release": 4, "makeup_db": 2.0},
        "ms_processor": {"width_pct": 110.0, "mono_below_hz": 100.0},
        "soft_clipper": {"ceiling_db": -0.5, "knee_pct": 40.0, "input_db": 2.0},
        "limiter": {"input_db": limiter_input_db, "ceiling_dbtp": CEILING_DBTP, "lookahead_ms": 2.0, "auto_release": 1},
    }
    y = x
    for name in CHAIN:
        if name == "limiter":
            y, rep = render_plugin(name, sr=sr, params=params[name], input=y, meter="loudness")
        else:
            y, rep = render_plugin(name, sr=sr, params=params[name], input=y)
        assert rep["nan_count"] == 0, name
    return y, rep["meter"]


def test_mastering_chain_target_and_ceiling(record_property):
    sr = 48000
    x = mix_fixture(sr)
    _, m0 = run_chain(x, sr, 0.0)
    gain = TARGET_LUFS - m0["lufs_i"]
    y, m = run_chain(x, sr, gain)
    record_property("chain_lufs_i", m["lufs_i"])
    record_property("chain_true_peak_dbtp", m["true_peak_dbtp"])
    record_property("chain_lra", m["lra"])
    assert abs(m["lufs_i"] - TARGET_LUFS) <= 0.5
    assert m["true_peak_dbtp"] <= CEILING_DBTP + 0.1
    assert np.max(np.abs(y)) <= 10 ** ((CEILING_DBTP + 0.1) / 20)


def test_mastering_chain_cpu_budget(record_property):  # "cpu_budget": CI runs it serially with the other timing gates
    """The six plugins' summed cost against the sum of their budgets: recorded, and gated at twice the sum so a
    runaway shows up without the timing noise of a shared machine failing the build. Release builds only, as the
    per-plugin gates (a Debug + sanitizer build read 69 % on a hosted runner)."""
    if os.environ.get("MM_PLUGIN_CONFIG", "Release") != "Release":
        pytest.skip("CPU budget is measured on Release builds only")
    total = 0.0
    budget = 0.0
    for name in CHAIN:
        r = bench_plugin(name, instances=50, seconds=2.0, reps=2)
        total += r["cpu_percent_per_instance"]
        budget += base_checks.BUDGETS[name]
    record_property("chain_cpu_percent_sum", total)
    record_property("chain_budget_sum", budget)
    assert total < 2.0 * budget
