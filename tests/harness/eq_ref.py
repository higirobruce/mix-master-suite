"""Independent reference for the Channel EQ's composed response (ADR-0003), built from the design mirrors."""
from __future__ import annotations

import numpy as np
from scipy import signal

from tests.harness import designs

SLOPES = [6, 12, 18, 24, 36, 48]
# Butterworth section Qs per ADR-0003 (one-pole marked as None)
SECTIONS = {6: [None], 12: [0.70710678], 18: [None, 1.0], 24: [0.54119610, 1.30656296],
            36: [0.51763809, 0.70710678, 1.93185165], 48: [0.50979558, 0.60134489, 0.89997622, 2.56291545]}


def onepole(kind: str, sr: float, fc: float):
    g = np.tan(np.pi * fc / sr)
    G = g / (1 + g)
    if kind == "lowpass":
        return np.array([G, G]), np.array([1.0, -(1 - 2 * G)])
    return np.array([1 - G, -(1 - G)]), np.array([1.0, -(1 - 2 * G)])


def response(params: dict, sr: float, f):
    """Complex response of the EQ at frequencies f for a params dict in real-world units (ids as in params.json)."""
    f = np.asarray(f, float)
    h = np.ones_like(f, dtype=complex)
    w = 2 * np.pi * f / sr
    analog_q = params.get("q_mode", 0) == 1

    def apply(b, a):
        nonlocal h
        h = h * signal.freqz(b, a, worN=w)[1]

    for side, kind in (("hp", "highpass"), ("lp", "lowpass")):
        if params.get(f"{side}_enabled", 0) >= 0.5:
            fc = params[f"{side}_freq_hz"]
            for q in SECTIONS[SLOPES[int(params.get(f"{side}_slope", 1))]]:
                if q is None:
                    apply(*onepole(kind, sr, fc))
                else:
                    apply(*designs.rbj(kind, sr, fc, 0.0, q))
    for n in range(1, 6):
        if params.get(f"b{n}_enabled", 1) < 0.5:
            continue
        g = params.get(f"b{n}_gain_db", 0.0)
        if abs(g) < 1e-9:
            continue
        kind = ["peak", "lowshelf", "highshelf"][int(params.get(f"b{n}_type", 0))]
        q = params.get(f"b{n}_q", 1.0)
        if analog_q and kind == "peak":
            q = q * (1 + abs(g) / 12.0)
        apply(*designs.matched(kind, sr, params[f"b{n}_freq_hz"], g, q))
    return h
