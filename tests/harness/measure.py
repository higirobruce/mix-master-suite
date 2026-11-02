"""Measurement helpers: levels, residuals, frequency/phase response."""
from __future__ import annotations

import numpy as np


def residual_dbfs(a: np.ndarray, b: np.ndarray) -> float:
    """20*log10 of the peak absolute difference (floored at -200 dBFS). Lengths are trimmed to the shorter."""
    n = min(len(a), len(b))
    d = np.max(np.abs(a[:n].astype(np.float64) - b[:n].astype(np.float64)))
    return float(20 * np.log10(d)) if d > 0 else -200.0


def rms_db(x: np.ndarray) -> float:
    x = np.asarray(x, dtype=np.float64)
    r = np.sqrt(np.mean(x * x)) if x.size else 0.0
    return float(20 * np.log10(r)) if r > 0 else -200.0


def _goertzel(x: np.ndarray, sr: float, freq: float) -> complex:
    n = np.arange(len(x))
    return complex(np.sum(x.astype(np.float64) * np.exp(-2j * np.pi * freq * n / sr)) * 2 / len(x))


def _segment(x: np.ndarray, sr: float, start_s: float, end_s: float | None) -> np.ndarray:
    a = int(start_s * sr)
    b = len(x) if end_s is None else int(end_s * sr)
    return x[a:b]


def level_at_db(x: np.ndarray, sr: float, freq: float, start_s: float = 0.0, end_s: float | None = None) -> float:
    seg = _segment(x, sr, start_s, end_s)
    return float(20 * np.log10(abs(_goertzel(seg, sr, freq)) + 1e-30))


def phase_at_deg(y: np.ndarray, ref: np.ndarray, sr: float, freq: float, start_s: float = 0.0,
                 end_s: float | None = None) -> float:
    a = _goertzel(_segment(y, sr, start_s, end_s), sr, freq)
    b = _goertzel(_segment(ref, sr, start_s, end_s), sr, freq)
    return float(np.degrees(np.angle(a / b)))


def freq_response(ir: np.ndarray, sr: float, nfft: int = 65536):
    """Magnitude (dB) and unwrapped phase (deg) of an impulse response."""
    h = np.fft.rfft(ir.astype(np.float64), n=nfft)
    f = np.fft.rfftfreq(nfft, 1 / sr)
    return f, 20 * np.log10(np.abs(h) + 1e-30), np.degrees(np.unwrap(np.angle(h)))


def max_step_ratio(x: np.ndarray) -> float:
    """Largest |x[n]/x[n-1]| over samples where the previous sample is non-zero."""
    x = np.asarray(x, dtype=np.float64)
    prev, cur = x[:-1], x[1:]
    mask = np.abs(prev) > 1e-9
    return float(np.max(np.abs(cur[mask] / prev[mask]))) if mask.any() else 1.0


def max_gain_step_db(y: np.ndarray, x: np.ndarray, min_level: float = 0.02) -> float:
    """Largest sample-to-sample change (dB) of the gain envelope y/x, over samples where |x| > min_level."""
    y = np.asarray(y, dtype=np.float64)
    x = np.asarray(x, dtype=np.float64)
    mask = np.abs(x) > min_level
    g = np.zeros_like(y)
    g[mask] = y[mask] / x[mask]
    valid = mask[1:] & mask[:-1] & (np.abs(g[:-1]) > 1e-9)
    if not valid.any():
        return 0.0
    r = np.abs(g[1:][valid] / g[:-1][valid])
    return float(np.max(np.abs(20 * np.log10(r + 1e-30))))


def envelope_step_db(y: np.ndarray, x: np.ndarray, sr: float, win_ms: float = 1.0, start_s: float = 0.0,
                     min_level_db: float = -90.0) -> float:
    """Largest window-to-window change (dB) of the RMS gain envelope y/x over win_ms windows (sine-safe click
    detector: a smooth parameter ramp moves a fraction of a dB per ms, a click moves many dB)."""
    y = np.asarray(y, dtype=np.float64)[int(start_s * sr):]
    x = np.asarray(x, dtype=np.float64)[int(start_s * sr):]
    w = max(1, int(sr * win_ms / 1000))
    n = (len(y) // w) * w
    ry = np.sqrt(np.mean(y[:n].reshape(-1, w) ** 2, axis=1))
    rx = np.sqrt(np.mean(x[:n].reshape(-1, w) ** 2, axis=1))
    g = 20 * np.log10(ry / (rx + 1e-20) + 1e-20)
    keep = (20 * np.log10(ry + 1e-20) > min_level_db)[1:] & (20 * np.log10(ry + 1e-20) > min_level_db)[:-1]
    d = np.abs(np.diff(g))[keep]
    return float(d.max()) if d.size else 0.0
