"""Stability fuzz (M2 Task 8): every plugin, every parameter automated with random values on noise while the sample
rate and block size change mid-stream. Audio seconds per plugin come from MM_FUZZ_SECONDS (default 120; the nightly
workflow runs 3600). Asserts: no NaN/Inf, peak below the input plus the plugin's maximum static gain plus 24 dB
(never below +30 dBFS; an unstable IIR on noise reaches float range within milliseconds, a stacked EQ does not), and
for pure-gain parameters no sample-to-sample gain jump above 6 dB."""
from __future__ import annotations

import os
from pathlib import Path

import numpy as np
import pytest

from tests.harness import measure, signals
from tests.harness.params import load_spec
from tests.harness.render import REPO_ROOT, render_plugin

PLUGINS = sorted(p.parent.name for p in (REPO_ROOT / "plugins").glob("*/params.json"))
FUZZ_SECONDS = float(os.environ.get("MM_FUZZ_SECONDS", "120"))
CHUNK_SECONDS = 20.0
POINT_INTERVAL_S = 0.05  # automation points per parameter (applied at block boundaries by the host)
INPUT_DB = -12.0
MAX_STATIC_GAIN_DB = {"utility": 24 + 12, "channel_eq": 5 * 18 + 12, "channel_comp": 24 + 12, "gate": 12}
PURE_GAIN = {"utility": ["gain_db", "trim_db"], "channel_eq": ["trim_db"], "channel_comp": ["makeup_db", "trim_db"],
             "gate": ["trim_db"]}


def random_value(p: dict, rng: np.random.Generator) -> float:
    if p["type"] in ("float", "int"):
        v = rng.uniform(p["min"], p["max"])
        return float(round(v)) if p["type"] == "int" else float(v)
    if p["type"] == "bool":
        return float(rng.integers(0, 2))
    return float(rng.integers(0, len(p["choices"])))


def fuzz_chunk(name: str, spec: dict, seconds: float, seed: int) -> dict:
    rng = np.random.default_rng(seed)
    sr = 96000  # the input's clock; the schedule switches the plugin between 44.1k, 96k and 48k
    x = signals.noise(sr, seconds, seed=seed, amp_db=INPUT_DB)
    times = np.arange(0.0, seconds, POINT_INTERVAL_S)
    automation = []
    for p in spec["params"]:
        if p["id"] == "bypass":
            continue  # bypass is exercised by the host-bypass check; keep the DSP running here
        rate = 1.0 if p["type"] in ("float", "int") else 0.2  # switches flip less often
        for t in times:
            if rng.random() < rate:
                automation.append({"param": p["id"], "t_s": float(t), "value": random_value(p, rng)})
    automation.sort(key=lambda a: a["t_s"])
    schedule = [{"t_s": 0.0, "sample_rate": 44100}, {"t_s": seconds / 3, "sample_rate": 96000},
                {"t_s": 2 * seconds / 3, "sample_rate": 48000}]
    blocks = [1, 16, 63, 64, 500, 4096, 7, 128]
    for i, t in enumerate(np.arange(0.0, seconds, 2.0)):
        schedule.append({"t_s": float(t), "block_size": blocks[i % len(blocks)]})
    schedule.sort(key=lambda s: s["t_s"])
    _, rep = render_plugin(name, sr=sr, block=64, params={}, input=x, automation=automation, schedule=schedule)
    return rep


@pytest.mark.parametrize("name", PLUGINS)
def test_fuzz_every_parameter(name, record_property):
    spec = load_spec(name)
    bound = max(30.0, INPUT_DB + MAX_STATIC_GAIN_DB.get(name, 12) + 24.0)
    done, chunk, peaks = 0.0, 0, []
    while done < FUZZ_SECONDS:
        seconds = min(CHUNK_SECONDS, FUZZ_SECONDS - done)
        rep = fuzz_chunk(name, spec, seconds, seed=1000 + chunk)
        assert rep["nan_count"] == 0, f"{name}: non-finite output in fuzz chunk {chunk}"
        assert rep["peak_db"] < bound, f"{name}: peak {rep['peak_db']:.1f} dBFS (bound {bound:.0f}) in fuzz chunk {chunk}"
        peaks.append(rep["peak_db"])
        done += seconds
        chunk += 1
    record_property("fuzz_seconds", done)
    record_property("fuzz_peak_db_max", float(max(peaks)))
    record_property("fuzz_peak_bound_db", bound)


@pytest.mark.parametrize("name", PLUGINS)
def test_pure_gain_parameters_never_jump(name, record_property):
    spec = load_spec(name)
    ids = PURE_GAIN.get(name, ["trim_db"])
    by_id = {p["id"]: p for p in spec["params"]}
    rng = np.random.default_rng(7)
    sr = 48000
    x = signals.sine(1000, sr, 4.0, amp_db=-12.0)
    automation = [{"param": pid, "t_s": float(t), "value": random_value(by_id[pid], rng)}
                  for pid in ids for t in np.arange(0.2, 4.0, 0.1)]
    automation.sort(key=lambda a: a["t_s"])
    y, rep = render_plugin(name, sr=sr, input=x, automation=automation)
    assert rep["nan_count"] == 0
    step = measure.max_gain_step_db(y[:, 0], x[:, 0])
    record_property("max_gain_step_db", float(step))
    assert step < 6.0, f"{name}: gain stepped {step:.2f} dB between samples"
