"""Parameter metadata from params.json (via tools/gen_params.py)."""
from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from gen_params import load_params  # noqa: E402


def load_spec(name: str) -> dict:
    return load_params(REPO_ROOT / "plugins" / name / "params.json")


def continuous(spec: dict) -> list[dict]:
    return [p for p in spec["params"] if p["type"] == "float" and p.get("smoothing_ms", 0) > 0]


def extremes(spec: dict) -> list[tuple[str, float]]:
    """(id, value) for min/max/default of every float/int, both bools, every choice index."""
    out = []
    for p in spec["params"]:
        if p["type"] in ("float", "int"):
            out += [(p["id"], p["min"]), (p["id"], p["max"]), (p["id"], p["default"])]
        elif p["type"] == "bool":
            out += [(p["id"], 0), (p["id"], 1)]
        else:
            out += [(p["id"], i) for i in range(len(p["choices"]))]
    return out
