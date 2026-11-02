#!/usr/bin/env python3
"""CPU benchmark: N instances at 48 kHz / 64 samples through mm_render --bench, compared to tools/budgets.json.

Usage: bench_cpu.py --plugin <id> [--instances 100] [--seconds 10]   |   bench_cpu.py --all
Exit 1 if any plugin exceeds its budget.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tests.harness.render import bench_plugin  # noqa: E402
from tests.harness.runner_speed import effective_budget, runner_factor  # noqa: E402


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--plugin")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--instances", type=int, default=100)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--sample-rate", type=int, default=48000)
    ap.add_argument("--block", type=int, default=64)
    a = ap.parse_args(argv)
    budgets = json.loads((ROOT / "tools" / "budgets.json").read_text())
    if a.all:
        plugins = sorted(p.name for p in (ROOT / "plugins").iterdir() if (p / "params.json").exists())
    elif a.plugin:
        plugins = [a.plugin]
    else:
        ap.error("--plugin <id> or --all")
    ok = True
    factor = runner_factor()
    print(f"runner speed factor vs reference machine: {factor:.2f} (limits scale by max(1, factor))")
    print(f"{'plugin':<18}{'instances':>10}{'cpu %/inst':>12}{'budget':>8}{'limit':>8}  result")
    for name in plugins:
        r = bench_plugin(name, instances=a.instances, seconds=a.seconds, sr=a.sample_rate, block=a.block)
        cpu = r["cpu_percent_per_instance"]
        budget = budgets.get(name)
        limit = effective_budget(budget)[0] if budget is not None else None
        passed = budget is None or cpu <= limit
        ok &= passed
        print(f"{name:<18}{a.instances:>10}{cpu:>12.4f}{(budget if budget is not None else float('nan')):>8}{(limit if limit is not None else float('nan')):>8.3f}  {'PASS' if passed else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
