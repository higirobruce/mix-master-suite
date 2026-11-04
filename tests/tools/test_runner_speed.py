"""The CPU-budget runner factor is the worst ratio over every calibration workload, so a runner that is slow on
the light dynamics plugins (what hurt the de-esser and Utility gates on hosted macOS runners while a compute-bound
calibration read it as fast) scales the limits by that slowdown; a faster machine gets no relief."""
from __future__ import annotations

import json

from tests.harness import runner_speed


def test_runner_factor_is_the_worst_workload_ratio(monkeypatch):
    ref = json.loads(runner_speed.REFERENCE.read_text())
    assert len(ref["workloads"]) >= 2, "a compute-bound and a dynamics calibration workload"
    costs = {w["workload"]: w["cpu_percent_per_instance"] for w in ref["workloads"]}
    names = list(costs)
    # the first workload reads 6 % faster than the reference, the second 28 % slower
    measured = {names[0]: costs[names[0]] * 0.94, names[1]: costs[names[1]] * 1.28}

    def fake_bench_min(cmd, reps):
        workload = cmd[cmd.index("--bench") + 1]
        return {"cpu_percent_per_instance": measured[workload]}

    monkeypatch.setattr(runner_speed, "bench_min", fake_bench_min)
    runner_speed.runner_factor.cache_clear()
    assert abs(runner_speed.runner_factor() - 1.28) < 1e-9
    limit, factor = runner_speed.effective_budget(0.4)
    assert abs(limit - 0.512) < 1e-9 and abs(factor - 1.28) < 1e-9
    measured[names[1]] = costs[names[1]] * 0.9  # both faster: no relief
    runner_speed.runner_factor.cache_clear()
    assert runner_speed.effective_budget(0.4) == (0.4, max(0.94, 0.9))
    runner_speed.runner_factor.cache_clear()
