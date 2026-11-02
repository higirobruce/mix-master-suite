"""Runner speed calibration for the CPU budgets.

The spec's budgets are per-instance percentages "on a mid-range laptop CPU". GitHub-hosted runners span a ~1.6x speed
range between draws (the same build read saturation 0.62 % and 1.00 % on two Linux runs, 0.69 % and 1.13 % on two
Windows runs), so an absolute gate flips with the draw. `runner_factor()` measures this machine with the core
benchmark (mm_bench_core: ns per sample of a 255-tap FIR, a double biquad and a TPT SVF) against the reference
machine recorded in docs/bench/core-2026-11-01.json, and the budget checks scale their limit by max(1, factor):
a faster machine gets no relief, a slower runner gets a proportional allowance. The factor is printed with every
result so a reviewer can see it (ruling recorded in the M3 review)."""
from __future__ import annotations

import json
import subprocess
from functools import lru_cache
from pathlib import Path

from tests.harness.render import REPO_ROOT, build_dir

REFERENCE = REPO_ROOT / "docs" / "bench" / "core-2026-11-01.json"
PROBES = ("fir_255_float", "biquad_double", "svf_float")


def bench_core_path() -> Path:
    for cand in (build_dir() / "tools" / "bench" / "mm_bench_core", build_dir() / "tools" / "bench" / "mm_bench_core.exe",
                 build_dir() / "tools" / "bench" / "Release" / "mm_bench_core.exe"):
        if cand.exists():
            return cand
    raise FileNotFoundError("mm_bench_core not built")


@lru_cache(maxsize=1)
def runner_factor() -> float:
    """Median ratio of this machine's core-module cost to the reference machine's (1.0 = same speed)."""
    ref = json.loads(REFERENCE.read_text())
    out = subprocess.run([str(bench_core_path())], capture_output=True, text=True, check=True).stdout
    measured = json.loads(out)
    ratios = sorted(measured[k] / ref[k] for k in PROBES if k in measured and k in ref and ref[k] > 0)
    return float(ratios[len(ratios) // 2]) if ratios else 1.0


def effective_budget(budget: float) -> tuple[float, float]:
    """(limit for this machine, runner factor)."""
    f = runner_factor()
    return budget * max(1.0, f), f
