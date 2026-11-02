"""Calibrate the Dattorro plate's RT60 label (ADR-0008): for each target label find the loop-formula seconds whose
Schroeder-measured T60 equals the label at 48 kHz, and print the table for DattorroPlate::correctedSeconds()."""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tests.core.test_reverb_modules import schroeder_t60  # noqa: E402
from tests.harness import signals  # noqa: E402
from tests.harness.render import render_module  # noqa: E402

TARGETS = [0.1, 0.2, 0.3, 0.5, 0.7, 1.0, 1.5, 2.0, 3.0, 5.0, 8.0, 12.0, 20.0]


def measure(raw_seconds: float, sr: int = 48000) -> float:
    dur = min(raw_seconds * 1.8 + 0.6, 16.0)
    x = signals.impulse(sr, dur, channels=2)
    y, _ = render_module("plate", sr=sr, params={"rt60_s": raw_seconds, "damping_pct": 0.0, "diffusion_pct": 70.0, "raw": 1}, input=x, channels=2)
    return schroeder_t60(y[:, 0] + y[:, 1], sr)


def main() -> None:
    rows = []
    for target in TARGETS:
        lo, hi = target * 0.3, target * 1.5
        for _ in range(18):  # bisection on the (monotone) measured T60
            mid = np.sqrt(lo * hi)
            m = measure(mid)
            if m < target:
                lo = mid
            else:
                hi = mid
            if hi / lo < 1.005:
                break
        raw = np.sqrt(lo * hi)
        rows.append((target, raw, measure(raw)))
        print(f"label {target:6.2f} s -> raw {raw:7.4f} s (measures {rows[-1][2]:6.3f})", flush=True)
    print("{" + ", ".join(f"{{{t:.1f}, {r:.4f}}}" for t, r, _ in rows) + "}")


if __name__ == "__main__":
    main()
