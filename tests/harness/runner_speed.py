"""Runner speed calibration for the CPU budgets.

The spec's budgets are per-instance percentages "on a mid-range laptop CPU". GitHub-hosted runners span a ~1.7x
range between draws for hosted plugins (one build read saturation 0.62 % and 1.00 % on two Linux runs, 0.69 % and
1.18 % on Windows), while an in-cache micro-benchmark reads those same runners as faster than the reference: the
variation is in the many-instance hosted workload (memory, cache, scheduling), not raw ALU speed. So the yardsticks
are fixed, non-product workloads of the plugins' shape, each 100 instances with their own buffers in the render
host: a compute-bound one (the 4x oversampled tanh module) and a dynamics one (the compressor core module: detector,
envelope and gain computer, the shape of the lightest plugins). docs/bench/calibration-2026-11-03.json records the
reference machine's
numbers. The budget checks scale their limit by max(1, worst measured / reference over the workloads): a faster
machine gets no relief, a slower runner gets a proportional allowance, and the factor is printed with every result
(ruling recorded in the M3 review; the dynamics workload was added in M5 after two hosted macOS runs read the
compute-bound calibration 6 % fast while the de-esser and Utility, the lightest plugins, read 20-28 % slow).

Both the calibration and the plugin benches take the minimum of REPS runs of SECONDS each (M4 review: one 5 s sample
of each read a hosted runner's calibration 25 % faster and Utility 20 % slower than the run before; interference only
adds time, so the minimum of a few runs is the cost and a single run is the cost plus the neighbours')."""
from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path

from tests.harness.render import REPO_ROOT, bench_min, mm_render_path

REFERENCE = REPO_ROOT / "docs" / "bench" / "calibration-2026-11-03.json"
REPS = 3
SECONDS = 3.0


@lru_cache(maxsize=1)
def runner_factor() -> float:
    """This machine's cost over the reference machine's, the worst ratio over the calibration workloads (1.0 = same)."""
    ref = json.loads(REFERENCE.read_text())
    worst = 0.0
    for w in ref["workloads"]:
        cmd = [str(mm_render_path()), "--bench", w["workload"], "--instances", str(ref["instances"]), "--seconds", str(ref["seconds"])]
        for kv in w.get("params", []):
            cmd += ["--set", kv]
        measured = bench_min(cmd, ref["reps"])["cpu_percent_per_instance"]
        worst = max(worst, float(measured) / float(w["cpu_percent_per_instance"]))
    return worst


def effective_budget(budget: float) -> tuple[float, float]:
    """(limit for this machine, runner factor)."""
    f = runner_factor()
    return budget * max(1.0, f), f
