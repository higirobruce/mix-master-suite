"""EBU Tech 3341 / 3342 compliance of the core loudness and true-peak meters, measured through mm_render's
`meter: loudness` report on a flat Utility render (M5 exit criterion: within 0.1 LU and 0.3 dBTP)."""
from __future__ import annotations

import numpy as np
import pyloudnorm
import pytest
import soundfile as sf

from tests.harness import ebu, signals
from tests.harness.matrix import SAMPLE_RATES as RATES
from tests.harness.render import render_plugin

PLUGIN = "utility"


def meter(x: np.ndarray, sr: int) -> dict:
    _, rep = render_plugin(PLUGIN, sr=sr, input=x, meter="loudness")
    return rep["meter"]


@pytest.mark.parametrize("sr", RATES)
@pytest.mark.parametrize("seq", sorted(ebu.SYNTH_3341))
def test_lufs_synthetic_vectors_4_rates(sr, seq):
    segments, expected = ebu.SYNTH_3341[seq]
    m = meter(ebu.synth(segments, sr), sr)
    for key, value in expected.items():
        assert abs(m[key] - value) <= ebu.TOL[key], f"seq-3341-{seq} at {sr}: {key} {m[key]:.2f} vs {value}"


@pytest.mark.parametrize("seq", sorted(ebu.SYNTH_3342))
def test_lra_synthetic_vectors(seq):
    segments, expected = ebu.SYNTH_3342[seq]
    m = meter(ebu.synth(segments, 48000), 48000)
    assert abs(m["lra"] - expected["lra"]) <= ebu.TOL["lra"], f"seq-3342-{seq}: LRA {m['lra']:.2f} vs {expected['lra']}"


@pytest.mark.parametrize("sr", RATES)
def test_truepeak_synthetic(sr):
    for name, x, dbtp in ebu.synth_truepeak_cases(sr):
        m = meter(x, sr)
        assert abs(m["true_peak_dbtp"] - dbtp) <= 0.1, f"{name} at {sr}: {m['true_peak_dbtp']:.2f} dBTP vs {dbtp}"


def test_cross_check_pyloudnorm():
    sr = 48000
    segments, _ = ebu.SYNTH_3341[3]
    x = ebu.synth(segments, sr)
    noise = signals.noise(sr, 20.0, seed=5, amp_db=-20.0)
    for sig in (x, noise):
        ref = pyloudnorm.Meter(sr).integrated_loudness(sig.astype(np.float64))
        assert abs(meter(sig, sr)["lufs_i"] - ref) <= 0.1


@pytest.mark.parametrize("wav,expected", ebu.real_vectors(), ids=lambda v: v.name if hasattr(v, "name") else "")
def test_ebu_real_vectors(wav, expected):
    x, sr = sf.read(wav, dtype="float32", always_2d=True)
    if x.shape[1] > 2:
        pytest.skip("multichannel sequence (the meters are measured in stereo in M5)")
    m = meter(x, int(sr))
    for key, value in expected.items():
        assert abs(m[key] - value) <= ebu.TOL[key], f"{wav.name}: {key} {m[key]:.2f} vs {value}"


def test_ebu_real_vectors_present_or_documented():
    if not ebu.real_vectors():
        pytest.skip("EBU files absent: run tools/fetch_ebu_vectors.py (see tests/fixtures/ebu/README.md)")
