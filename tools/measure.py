#!/usr/bin/env python3
"""Run a plugin's measurement suite and print the recorded numbers.

Usage: measure.py --plugin <id> (--all | --test <pytest -k expression>) [--module <core module>]
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--plugin")
    ap.add_argument("--module")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--test", help="pytest -k expression")
    a = ap.parse_args(argv)
    if a.plugin:
        target = ROOT / "tests" / "plugins" / f"test_{a.plugin}.py"
    elif a.module:
        target = ROOT / "tests" / "core" / f"test_{a.module}.py"
    else:
        ap.error("--plugin <id> or --module <name>")
    if not target.exists():
        print(f"no test module {target}", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as td:
        report = Path(td) / "report.json"
        cmd = [sys.executable, "-m", "pytest", str(target), "-q", "-p", "tests.harness.report_plugin",
               f"--mm-report={report}"]
        if a.test:
            cmd += ["-k", a.test]
        rc = subprocess.run(cmd, cwd=ROOT).returncode
        if report.exists():
            rows = json.loads(report.read_text())
            print(f"\n{'test':<48}{'property':<34}{'value':>14}")
            for r in rows:
                print(f"{r['test']:<48}{r['name']:<34}{r['value']:>14.4f}" if isinstance(r["value"], (int, float))
                      else f"{r['test']:<48}{r['name']:<34}{str(r['value']):>14}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
