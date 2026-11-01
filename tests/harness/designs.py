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


def analog_prototype(kind: str, f0: float, gain_db: float, q: float, f):
    """|Ha(j w/w0)|^2 of the RBJ analog prototypes (peak / lowshelf / highshelf / lowpass / highpass)."""
    s = 1j * np.asarray(f, float) / f0
    A = 10 ** (gain_db / 40)
    if kind == "peak":
        h = (s * s + s * A / q + 1) / (s * s + s / (A * q) + 1)
    elif kind == "lowshelf":
        h = A * (s * s + np.sqrt(A) / q * s + A) / (A * s * s + np.sqrt(A) / q * s + 1)
    elif kind == "highshelf":
        h = A * (A * s * s + np.sqrt(A) / q * s + 1) / (s * s + np.sqrt(A) / q * s + A)
    elif kind == "lowpass":
        h = 1 / (s * s + s / q + 1)
    elif kind == "highpass":
        h = s * s / (s * s + s / q + 1)
    else:
        raise ValueError(kind)
    return np.abs(h) ** 2


def matched(kind: str, sr: float, f0: float, gain_db: float = 0.0, q: float = 0.707):
    """Vicanek-style matched biquad: poles by impulse invariance of the analog denominator, numerator chosen so
    |H|^2 equals the analog prototype at DC, Nyquist and f0 (lowpass/highpass per Vicanek 2016)."""
    w0 = 2 * np.pi * f0 / sr
    A = 10 ** (gain_db / 40)
    if kind == "peak":
        wp, zeta = w0, 1 / (2 * A * q)
    elif kind == "lowshelf":
        wp, zeta = w0 / np.sqrt(A), 1 / (2 * q)
    elif kind == "highshelf":
        wp, zeta = w0 * np.sqrt(A), 1 / (2 * q)
    else:
        wp, zeta = w0, 1 / (2 * q)
    a2 = np.exp(-2 * zeta * wp)
    a1 = -2 * np.exp(-zeta * wp) * (np.cos(wp * np.sqrt(1 - zeta * zeta)) if zeta <= 1 else np.cosh(wp * np.sqrt(zeta * zeta - 1)))
    A0, A1, A2 = (1 + a1 + a2) ** 2, (1 - a1 + a2) ** 2, -4 * a2
    phi1 = np.sin(w0 / 2) ** 2
    phi0 = 1 - phi1
    phi2 = 4 * phi0 * phi1
    D0 = A0 * phi0 + A1 * phi1 + A2 * phi2  # |D|^2 at w0
    if kind == "lowpass":
        B0 = A0
        R1 = D0 * q * q
        B1 = (R1 - B0 * phi0) / phi1
        b0 = 0.5 * (np.sqrt(B0) + np.sqrt(B1))
        b1 = np.sqrt(B0) - b0
        b2 = 0.0
    elif kind == "highpass":
        b0 = q * np.sqrt(D0) / (4 * phi1)
        b1, b2 = -2 * b0, b0
    else:
        if wp >= 0.98 * np.pi:
            return rbj(kind, sr, f0, gain_db, q)  # no real matched solution: bilinear fallback (as in C++)
        T0 = analog_prototype(kind, f0, gain_db, q, 0.0)
        Tn = analog_prototype(kind, f0, gain_db, q, sr / 2)
        Tw = analog_prototype(kind, f0, gain_db, q, f0)
        B0, B1 = A0 * T0, A1 * Tn
        B2 = (Tw * D0 - B0 * phi0 - B1 * phi1) / phi2
        W = 0.5 * (np.sqrt(B0) + np.sqrt(B1))
        if W * W + B2 < 0:
            return rbj(kind, sr, f0, gain_db, q)
        b0 = 0.5 * (W + np.sqrt(W * W + B2))
        b1 = 0.5 * (np.sqrt(B0) - np.sqrt(B1))
        b2 = -B2 / (4 * b0)
    return np.array([b0, b1, b2]), np.array([1.0, a1, a2])
