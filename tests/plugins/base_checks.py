"""Checks every plugin test module runs (Review Focus items of the M0 plan). Each asserts and returns the number."""
from __future__ import annotations

import json
import os
from pathlib import Path

import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.params import continuous, extremes, load_spec
from tests.harness.render import REPO_ROOT, bench_plugin, render_plugin

BUDGETS = json.loads((REPO_ROOT / "tools" / "budgets.json").read_text())


def check_variable_block_sizes(name: str, params: dict | None = None, tol_dbfs: float = -120.0) -> float:
    sr = 48000
    x = signals.noise(sr, 1.0, seed=11)
    ref, _ = render_plugin(name, sr=sr, block=4096, params=params, input=x)
    sched = [{"t_s": t, "block_size": b} for t, b in zip(np.linspace(0, 0.9, 7), [1, 16, 63, 64, 500, 4096, 7])]
    var, rep = render_plugin(name, sr=sr, block=64, params=params, input=x, schedule=sched)
    assert rep["nan_count"] == 0
    res = measure.residual_dbfs(var, ref)
    assert res < tol_dbfs, f"variable block sizes residual {res:.1f} dBFS"
    return res


def check_sr_change_mid_stream(name: str, params: dict | None = None, tol_dbfs: float = -120.0) -> float:
    """Schedule 48k -> 96k at 1 s; the post-change audio must match a fresh 96k render of the same material."""
    x = signals.noise(96000, 2.0, seed=12)
    y, rep = render_plugin(name, sr=96000, params=params, input=x, schedule=[{"t_s": 0.0, "sample_rate": 48000},
                                                                             {"t_s": 1.0, "sample_rate": 96000}])
    assert rep["nan_count"] == 0
    ref, _ = render_plugin(name, sr=96000, params=params, input=x)
    start = int(1.1 * 96000)
    res = measure.residual_dbfs(y[start:], ref[start:])
    assert res < tol_dbfs, f"sample-rate change residual {res:.1f} dBFS"
    return res


def check_state_roundtrip(name: str, params: dict | None = None) -> None:
    spec = load_spec(name)
    if params is None:
        params = {}
        for p in spec["params"]:
            if p["type"] in ("float", "int"):
                params[p["id"]] = p["min"] + 0.37 * (p["max"] - p["min"])
            elif p["type"] == "bool":
                params[p["id"]] = 1 if p["id"] not in ("bypass", "mute_l", "mute_r") else 0
            else:
                params[p["id"]] = min(1, len(p["choices"]) - 1)
    x = signals.noise(48000, 0.5, seed=13)
    a, _ = render_plugin(name, sr=48000, params=params, input=x)
    b, _ = render_plugin(name, sr=48000, params=params, input=x, state_roundtrip=True)
    res = measure.residual_dbfs(a, b)
    assert res < -120, f"state round trip changed the output: {res:.1f} dBFS"


def check_param_extremes(name: str, max_peak_db: float = 24.5, ramp_params: dict[str, float] | None = None) -> None:
    """Every parameter jumped to min/max/default (each choice, both bools) mid-signal: finite, bounded, and for the
    pure-gain parameters listed in ramp_params the gain envelope never steps more than the given dB per sample."""
    spec = load_spec(name)
    sr = 48000
    x = signals.sine(1000, sr, 1.0, amp_db=-12)
    smoothed = {p["id"] for p in continuous(spec)}
    ramp_params = ramp_params or {}
    for pid, value in extremes(spec):
        y, rep = render_plugin(name, sr=sr, input=x, automation=[{"param": pid, "t_s": 0.5, "value": value}])
        assert rep["nan_count"] == 0, f"{pid}={value} produced non-finite samples"
        assert rep["peak_db"] < max_peak_db, f"{pid}={value} peak {rep['peak_db']:.1f} dB"
        if pid in ramp_params:
            assert pid in smoothed, f"{pid} is not a smoothed parameter"
            step = measure.max_gain_step_db(y[:, 0], x[:, 0])
            assert step < ramp_params[pid], f"{pid}={value} click: gain step {step:.3f} dB/sample"


def check_denormal_tail(name: str, params: dict | None = None) -> None:
    sr = 48000
    loud = signals.sine(1000, sr, 2.0, amp_db=-6)
    silent = np.zeros((2 * sr, 2), dtype=np.float32)
    _, rep_loud = render_plugin(name, sr=sr, params=params, input=loud)
    y, rep = render_plugin(name, sr=sr, params=params, input=np.concatenate([loud, silent]))
    assert rep["nan_count"] == 0
    tail_ratio = rep["realtime_ratio"]
    assert tail_ratio < 2.0 * max(rep_loud["realtime_ratio"], 1e-4) + 1e-3, "silence after audio runs slower (denormals?)"
    assert np.max(np.abs(y[int(3.5 * sr):])) < 1e-6


def check_channel_configs(name: str, params: dict | None = None) -> None:
    sr = 48000
    mono = signals.sine(1000, sr, 0.2, amp_db=-12, channels=1)
    y, _ = render_plugin(name, sr=sr, params=params, input=mono, layout={"in": 1, "out": 1})
    assert y.shape[1] == 1 and np.max(np.abs(y)) > 0.01
    y, _ = render_plugin(name, sr=sr, params=params, input=mono, layout={"in": 1, "out": 2})
    assert y.shape[1] == 2 and np.max(np.abs(y[:, 1])) > 0.01
    stereo = signals.sine(1000, sr, 0.2, amp_db=-12, channels=2)
    y, _ = render_plugin(name, sr=sr, params=params, input=stereo, layout={"in": 2, "out": 2})
    assert y.shape[1] == 2


def check_cpu_budget(name: str, instances: int = 100, seconds: float = 5.0) -> float:
    """Timing is only meaningful on an optimised build run serially (CI runs this test on its own)."""
    if os.environ.get("MM_PLUGIN_CONFIG", "Release") != "Release":
        pytest.skip("CPU budget is measured on Release builds only")
    r = bench_plugin(name, instances=instances, seconds=seconds)
    cpu = r["cpu_percent_per_instance"]
    budget = BUDGETS[name]
    assert cpu <= budget, f"{name}: {cpu:.4f} % per instance exceeds budget {budget}"
    return cpu
