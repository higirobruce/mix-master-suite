"""Sample-rate matrix helpers for the core measurement tests."""
from __future__ import annotations

import numpy as np

SAMPLE_RATES = [44100, 48000, 96000, 192000]


def measure_band(sr: int) -> tuple[float, float]:
    """20 Hz to min(20 kHz, 0.45·fs)."""
    return 20.0, min(20000.0, 0.45 * sr)


def assert_response(f, mag_db, phase_deg, ref_mag_db, ref_phase_deg, band, tol_db=0.1, tol_deg=2.0, label=""):
    """Assert |mag − ref| ≤ tol_db and the wrapped phase difference ≤ tol_deg over band. Returns (max_db, max_deg)."""
    f = np.asarray(f)
    sel = (f >= band[0]) & (f <= band[1])
    d_mag = np.abs(np.asarray(mag_db)[sel] - np.asarray(ref_mag_db)[sel])
    d_ph = (np.asarray(phase_deg)[sel] - np.asarray(ref_phase_deg)[sel] + 180.0) % 360.0 - 180.0
    max_db, max_deg = float(d_mag.max()), float(np.abs(d_ph).max())
    assert max_db <= tol_db, f"{label} magnitude deviates {max_db:.3f} dB (> {tol_db}) at {f[sel][d_mag.argmax()]:.1f} Hz"
    assert max_deg <= tol_deg, f"{label} phase deviates {max_deg:.2f}° (> {tol_deg}) at {f[sel][np.abs(d_ph).argmax()]:.1f} Hz"
    return max_db, max_deg
