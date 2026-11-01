"""Test signal generators (frames x channels float32)."""
from __future__ import annotations

from pathlib import Path

import numpy as np
import soundfile as sf


def _amp(amp_db: float) -> float:
    return float(10 ** (amp_db / 20))


def sine(freq: float, sr: int, dur: float, amp_db: float = 0.0, channels: int = 2) -> np.ndarray:
    t = np.arange(int(round(dur * sr))) / sr
    x = (_amp(amp_db) * np.sin(2 * np.pi * freq * t)).astype(np.float32)
    return np.repeat(x[:, None], channels, axis=1)


def impulse(sr: int, dur: float, channels: int = 2) -> np.ndarray:
    x = np.zeros((int(round(dur * sr)), channels), dtype=np.float32)
    x[0, :] = 1.0
    return x


def noise(sr: int, dur: float, seed: int = 0, amp_db: float = -12.0, channels: int = 2) -> np.ndarray:
    rng = np.random.default_rng(seed)
    return (_amp(amp_db) * rng.uniform(-1.0, 1.0, size=(int(round(dur * sr)), channels))).astype(np.float32)


def dc(sr: int, dur: float, level: float = 1.0, channels: int = 2) -> np.ndarray:
    return np.full((int(round(dur * sr)), channels), level, dtype=np.float32)


def write_wav(path: Path, x: np.ndarray, sr: int) -> None:
    sf.write(str(path), x.astype(np.float32), sr, subtype="FLOAT")
