import numpy as np
import pytest

from tests.harness import signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module


def edge_ms(env_db, sr, start_db, end_db):
    """10 %-90 % transition time in ms of an envelope (dB) between two levels."""
    a = start_db + 0.1 * (end_db - start_db)
    b = start_db + 0.9 * (end_db - start_db)
    if end_db > start_db:
        t10 = np.argmax(env_db >= a)
        t90 = np.argmax(env_db >= b)
    else:
        t10 = np.argmax(env_db <= a)
        t90 = np.argmax(env_db <= b)
    return 1000.0 * (t90 - t10) / sr


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("attack_ms,release_ms", [(1.0, 50.0), (10.0, 300.0), (100.0, 1000.0)])
def test_envelope_module_attack_release_on_tone_burst(sr, attack_ms, release_ms, record_property):
    """Module 'envelope' outputs the follower's envelope (dB/100) for a DC burst (0.3 s on from 0.1 s): the peak
    detector then sees a clean step, so the 10-90 % times are the follower's own."""
    n = int((0.5 + 4.0 * release_ms / 1000.0) * sr)  # long enough for the 90 % release point
    t = np.arange(n) / sr
    x = (1.0 * ((t >= 0.1) & (t < 0.4))).astype(np.float32)[:, None]
    y, rep = render_module("envelope", sr=sr, params={"mode": 0, "attack_ms": attack_ms, "release_ms": release_ms}, input=x)
    assert rep["nan_count"] == 0
    env = y[:, 0].astype(np.float64) * 100.0
    on = env[int(0.1 * sr):int(0.4 * sr)]
    off = env[int(0.4 * sr):]
    top = np.max(on)
    FLOOR = -120.0  # the follower's silence floor (gainToDb(0)): the release target, and where the attack starts
    atk = edge_ms(on, sr, FLOOR, top)
    rel = edge_ms(off, sr, top, FLOOR)
    record_property("attack_ms_measured", float(atk))
    record_property("release_ms_measured", float(rel))
    assert abs(atk - attack_ms) <= 0.1 * attack_ms + 2000.0 / sr
    assert abs(rel - release_ms) <= 0.1 * release_ms + 2000.0 / sr
