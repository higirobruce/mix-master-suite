"""EBU Tech 3341 / 3342 test vectors.

The synthetic sequences below follow the signal definitions in EBU Tech 3341 (v3) Table 1 and Tech 3342 Table 1:
stereo 1 kHz sines at the stated levels (dBFS per channel) and durations. They are exact by construction, so they run
on every CI runner. The programme-material sequences (3341-7/8, 3342-5/6) and the true-peak sequences (3341-15..23)
are only available as the EBU's own files: tools/fetch_ebu_vectors.py puts them in tests/fixtures/ebu/ and
`real_vectors()` lists what is there with the expected readings from the EBU documents.
"""
from __future__ import annotations

import re
from pathlib import Path

import numpy as np

from tests.harness import signals

REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURES = REPO_ROOT / "tests" / "fixtures" / "ebu"

# (sequence, [(level dBFS, seconds), ...], expected) — expected keys: lufs_i, lufs_m_max, lufs_s_max, lra, tol
SYNTH_3341 = {
    1: ([(-23.0, 20.0)], {"lufs_i": -23.0}),
    2: ([(-33.0, 20.0)], {"lufs_i": -33.0}),
    3: ([(-36.0, 10.0), (-23.0, 60.0), (-36.0, 10.0)], {"lufs_i": -23.0}),
    4: ([(-72.0, 10.0), (-36.0, 10.0), (-23.0, 60.0), (-36.0, 10.0), (-72.0, 10.0)], {"lufs_i": -23.0}),
    5: ([(-26.0, 20.0), (-20.0, 20.1), (-26.0, 20.0)], {"lufs_i": -23.0}),
    9: ([(-20.0, 1.34), (-30.0, 1.66)] * 5, {"lufs_s_max": -23.0}),      # Tech 3341 seq-9: short-term, 3 s period
}
SYNTH_3342 = {
    1: ([(-20.0, 20.0), (-30.0, 20.0)], {"lra": 10.0}),
    2: ([(-20.0, 20.0), (-15.0, 20.0)], {"lra": 5.0}),
    3: ([(-40.0, 20.0), (-20.0, 20.0)], {"lra": 20.0}),
    4: ([(-50.0, 20.0), (-35.0, 20.0), (-20.0, 20.0), (-35.0, 20.0), (-50.0, 20.0)], {"lra": 15.0}),
}
TOL = {"lufs_i": 0.1, "lufs_m_max": 0.1, "lufs_s_max": 0.1, "lra": 1.0, "true_peak_dbtp": 0.3}


def synth(segments: list[tuple[float, float]], sr: int) -> np.ndarray:
    parts = [signals.sine(1000.0, sr, dur, amp_db=level, channels=2) for level, dur in segments]
    return np.concatenate(parts, axis=0).astype(np.float32)


def synth_truepeak_cases(sr: int) -> list[tuple[str, np.ndarray, float]]:
    """(name, stereo signal, expected dBTP): a sine at fs / 4 whose samples all sit at 0.707 of the amplitude
    (true peak -6.02 dBTP, sample peak -9.03), and a 1 kHz sine at -1 dBFS."""
    n = sr
    t = np.arange(n)
    quarter = 0.5 * np.sin(2 * np.pi * 0.25 * t + np.pi / 4)
    one = 10 ** (-1 / 20) * np.sin(2 * np.pi * 1000.0 * t / sr)
    return [("fs/4 inter-sample", np.stack([quarter, -quarter], axis=1).astype(np.float32), -6.02),
            ("1 kHz -1 dBFS", np.stack([one, one], axis=1).astype(np.float32), -1.0)]


# Expected readings for the EBU files (EBU Tech 3341 v3 Table 1 / Table 2, Tech 3342 Table 1). Keys are the
# sequence tags found in the file names.
REAL_EXPECTED = {
    "3341-1": {"lufs_i": -23.0}, "3341-2": {"lufs_i": -33.0}, "3341-3": {"lufs_i": -23.0}, "3341-4": {"lufs_i": -23.0},
    "3341-5": {"lufs_i": -23.0}, "3341-6": {"lufs_i": -23.0}, "3341-7": {"lufs_i": -23.0}, "3341-8": {"lufs_i": -23.0},
    "3341-9": {"lufs_s_max": -23.0}, "3341-10": {"lufs_s_max": -23.0}, "3341-11": {"lufs_s_max": -38.0},
    "3341-12": {"lufs_m_max": -23.0}, "3341-13": {"lufs_m_max": -23.0}, "3341-14": {"lufs_m_max": -38.0},
    "3341-15": {"true_peak_dbtp": -6.0}, "3341-16": {"true_peak_dbtp": -6.0}, "3341-17": {"true_peak_dbtp": -6.0},
    "3341-18": {"true_peak_dbtp": -6.0}, "3341-19": {"true_peak_dbtp": 3.0}, "3341-20": {"true_peak_dbtp": 0.0},
    "3341-21": {"true_peak_dbtp": 0.0}, "3341-22": {"true_peak_dbtp": 0.0}, "3341-23": {"true_peak_dbtp": 0.0},
    "3342-1": {"lra": 10.0}, "3342-2": {"lra": 5.0}, "3342-3": {"lra": 20.0}, "3342-4": {"lra": 15.0},
    "3342-5": {"lra": 5.0}, "3342-6": {"lra": 15.0},
}


def real_vectors() -> list[tuple[Path, dict]]:
    """Every EBU file present in the fixture folder with its expected readings (a file may carry two sequences:
    seq-3341-7_seq-3342-5 is both the programme loudness and the LRA test)."""
    out = []
    if not FIXTURES.is_dir():
        return out
    for wav in sorted(FIXTURES.glob("*.wav")):
        expected: dict = {}
        for tag in re.findall(r"seq-(334[12]-\d+)", wav.name):
            expected.update(REAL_EXPECTED.get(tag, {}))
        if expected:
            out.append((wav, expected))
    return out
