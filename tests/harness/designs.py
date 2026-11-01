"""Reference filter designs mirrored in Python (RBJ cookbook), used to check the C++ designs via scipy."""
from __future__ import annotations

import numpy as np
from scipy import signal


def rbj(kind: str, sr: float, f0: float, gain_db: float = 0.0, q: float = 0.707):
    """Return (b, a) for RBJ peak/lowshelf/highshelf/lowpass/highpass/notch/allpass."""
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / sr
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / (2 * q)
    if kind == "peak":
        b = [1 + alpha * A, -2 * cw, 1 - alpha * A]
        a = [1 + alpha / A, -2 * cw, 1 - alpha / A]
    elif kind in ("lowshelf", "highshelf"):
        s = 1 if kind == "lowshelf" else -1
        sa = 2 * np.sqrt(A) * alpha
        b = [A * ((A + 1) - s * (A - 1) * cw + sa), 2 * s * A * ((A - 1) - s * (A + 1) * cw), A * ((A + 1) - s * (A - 1) * cw - sa)]
        a = [(A + 1) + s * (A - 1) * cw + sa, -2 * s * ((A - 1) + s * (A + 1) * cw), (A + 1) + s * (A - 1) * cw - sa]
    elif kind == "lowpass":
        b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "highpass":
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "notch":
        b = [1, -2 * cw, 1]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "allpass":
        b = [1 - alpha, -2 * cw, 1 + alpha]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    else:
        raise ValueError(kind)
    b, a = np.asarray(b, float), np.asarray(a, float)
    return b / a[0], a / a[0]


def freqz_db_deg(b, a, f, sr):
    w, h = signal.freqz(b, a, worN=2 * np.pi * np.asarray(f) / sr)
    return 20 * np.log10(np.abs(h) + 1e-300), np.degrees(np.angle(h))
