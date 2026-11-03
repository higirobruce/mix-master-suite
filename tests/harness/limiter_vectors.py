"""Synthetic worst cases for the true-peak limiter's ceiling gate (M5 Task 8): inter-sample peaks, bursts at the
top of the band, square waves, loud noise, a DC-offset tone and click trains, at any rate. Each vector is stereo and
2 s long; the EBU true-peak files join the set when tests/fixtures/ebu holds them."""
from __future__ import annotations

import numpy as np

from tests.harness import ebu, signals


def vectors(sr: int) -> list[tuple[str, np.ndarray]]:
    t = np.arange(int(2.0 * sr)) / sr
    out = []
    # 0.45 fs bursts at +3 dBFS, 50 ms on / 50 ms off, with 2 ms cosine edges: a hard-gated near-Nyquist tone makes
    # a 32x resampler ring 0.4 dB over the tone's own level at every edge, which measures the estimator, not the plugin
    gate = ((t * 10) % 1.0) < 0.5
    env = gate.astype(np.float64)
    fade = max(1, int(0.002 * sr))
    ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(fade) / fade)
    env[:fade] = ramp  # the first burst starts at t = 0: fade it in like the others
    edges = np.flatnonzero(np.diff(gate.astype(int)))
    for e in edges:
        if gate[e + 1]:  # turning on
            env[e + 1:e + 1 + fade] = ramp[: max(0, min(fade, len(env) - e - 1))]
        else:            # turning off
            env[e + 1 - fade:e + 1] = ramp[::-1][: fade]
    burst = 10 ** (3 / 20) * np.sin(2 * np.pi * 0.45 * sr * t) * env
    out.append(("0.45fs bursts +3 dBFS", np.stack([burst, burst], axis=1)))
    # fs / 4 with the samples at 0.707 of the amplitude: +3 dB of inter-sample peak at +2 dBFS
    isp = 10 ** (2 / 20) * np.sin(2 * np.pi * 0.25 * sr * t + np.pi / 4)
    out.append(("fs/4 inter-sample +2 dBFS", np.stack([isp, -isp], axis=1)))
    # 1 kHz square at +6 dBFS
    sq = 10 ** (6 / 20) * np.sign(np.sin(2 * np.pi * 1000.0 * t))
    out.append(("1 kHz square +6 dBFS", np.stack([sq, sq], axis=1)))
    # white noise at +12 dBFS peak-ish (RMS -2)
    rng = np.random.default_rng(17)
    noise = rng.standard_normal(len(t)) * 10 ** (-2 / 20)
    out.append(("noise +12 dBFS peaks", np.stack([noise, rng.standard_normal(len(t)) * 10 ** (-2 / 20)], axis=1)))
    # a 100 Hz tone at 0 dBFS riding on +0.5 DC
    dc = 0.5 + np.sin(2 * np.pi * 100.0 * t)
    out.append(("100 Hz on +0.5 DC", np.stack([dc, dc], axis=1)))
    # click train: single-sample +6 dBFS impulses every 20 ms
    clicks = np.zeros(len(t))
    clicks[:: int(0.02 * sr)] = 10 ** (6 / 20)
    out.append(("click train +6 dBFS", np.stack([clicks, clicks], axis=1)))
    # a 60 Hz tone at +9 dBFS: sustained programme-like overload
    low = 10 ** (9 / 20) * np.sin(2 * np.pi * 60.0 * t)
    out.append(("60 Hz +9 dBFS", np.stack([low, low * 0.9], axis=1)))
    return [(name, x.astype(np.float32)) for name, x in out]


def ebu_truepeak_files() -> list:
    """(path, expected dBTP) of the EBU true-peak sequences present (3341-15 .. 23)."""
    return [(wav, exp["true_peak_dbtp"]) for wav, exp in ebu.real_vectors() if "true_peak_dbtp" in exp]
