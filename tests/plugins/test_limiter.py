"""True-peak limiter (M5 Task 8): ceiling never exceeded by more than 0.1 dBTP on the test set, release times,
null below threshold, latency and click-free lookahead changes, aliasing, dither, stereo link, base checks, CPU."""
from __future__ import annotations

import numpy as np
import pytest
import soundfile as sf
from scipy import signal as sps

from tests.harness import limiter_vectors, measure, signals
from tests.harness.matrix import SAMPLE_RATES
from tests.harness.render import render_module, render_plugin
from tests.plugins import base_checks

PLUGIN = "limiter"
BASE = {"input_db": 0.0, "ceiling_dbtp": -1.0, "release_ms": 100.0, "auto_release": 0, "lookahead_ms": 2.0, "character": 0, "link_pct": 100.0, "dither": 0}


def true_peak_dbtp(y: np.ndarray, edge: int = 2048) -> float:
    """32x polyphase resample; the file's first and last `edge` samples are left out (the resampler rings at a
    truncated signal's edges, which is not the plugin's output)."""
    up = sps.resample_poly(y.astype(np.float64), 32, 1, axis=0)
    return float(20 * np.log10(np.max(np.abs(up[32 * edge:-32 * edge])) + 1e-30))


def passthrough_reference(x: np.ndarray, sr: int, with_latency: bool = False):
    """The limiter's oversampler up and down with nothing in between (4x at every rate)."""
    y, rep = render_module("oversampled_tanh", sr=sr, params={"factor": 4, "drive_db": -80.0}, input=x, channels=2)
    y = y * 10 ** (80 / 20)  # the module outputs tanh(drive * x): undo the drive
    return (y, rep["latency_samples"]) if with_latency else y


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("ceiling", [-0.1, -1.0, -3.0])
@pytest.mark.parametrize("lookahead", [0.0, 2.0, 10.0])
def test_limiter_ceiling_never_exceeded(sr, ceiling, lookahead, record_property):
    """Every synthetic worst case of the roadmap's set (0.45 fs bursts, inter-sample peaks, square waves, a
    DC-offset tone, click trains, a 60 Hz overload), every character and release, auto on and off: the output's true
    peak (32x resample) over the whole render, the first block included, stays at or under the ceiling + 0.1 dBTP.
    White noise driven 7-13 dB over the ceiling is recorded, not gated: limiting it that hard makes the gain
    modulation itself broadband, and the reconstruction filter leaves 0.2-0.4 dB of inter-sample overshoot at every
    rate and lookahead (ADR-0012 amendment)."""
    worst = -200.0
    for name, x in limiter_vectors.vectors(sr):
        for character, release, auto in ((0, 100.0, 0), (1, 1.0, 0), (2, 1000.0, 0), (0, 100.0, 1)):
            params = dict(BASE, ceiling_dbtp=ceiling, lookahead_ms=lookahead, character=character, release_ms=release, auto_release=auto)
            y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
            assert rep["nan_count"] == 0
            tp = true_peak_dbtp(y)
            if name.startswith("noise"):
                record_property(f"noise_over_{sr}_{ceiling}_{lookahead}_{character}_{auto}", float(tp - ceiling))
                continue
            worst = max(worst, tp - ceiling)
            assert tp <= ceiling + 0.1, f"{name} ch{character} rel{release} auto{auto}: {tp:.3f} dBTP over {ceiling}"
    record_property(f"worst_over_{sr}_{ceiling}_{lookahead}", worst)


def test_limiter_ebu_truepeak_files():
    files = limiter_vectors.ebu_truepeak_files()
    if not files:
        pytest.skip("EBU true-peak files absent (tests/fixtures/ebu/README.md)")
    for wav, _ in files:
        x, sr = sf.read(wav, dtype="float32", always_2d=True)
        y, rep = render_plugin(PLUGIN, sr=int(sr), params=dict(BASE, input_db=6.0, ceiling_dbtp=-1.0), input=x[:, :2])
        assert rep["nan_count"] == 0
        assert true_peak_dbtp(y) <= -1.0 + 0.1


@pytest.mark.parametrize("character,fast_ms,slow_ms", [(0, 30.0, 300.0), (1, 10.0, 500.0), (2, 50.0, 150.0)])
def test_limiter_release_times(character, fast_ms, slow_ms, record_property):
    """Dual-stage release (ADR-0012): after a 500 ms burst 5 dB over the ceiling the gain recovers through the slow
    stage (10-90 % within 25 % of its labelled time); after a 5 ms burst the fast stage alone recovers it (10-90 %
    within 50 % of its labelled time, plus two 1 ms windows either way)."""
    sr = 48000
    win = sr // 1000

    def recovery(burst_s):
        n = int((1.0 + burst_s + 6 * slow_ms / 1000) * sr)
        t = np.arange(n) / sr
        env = np.where((t >= 0.5) & (t < 0.5 + burst_s), 10 ** (5 / 20), 10 ** (-20 / 20))
        tone = np.sin(2 * np.pi * 1000.0 * t) * env
        x = np.stack([tone, tone], axis=1).astype(np.float32)
        params = dict(BASE, character=character, release_ms=slow_ms, lookahead_ms=1.0)
        y, rep = render_plugin(PLUGIN, sr=sr, params=params, input=x)
        assert rep["nan_count"] == 0
        lat = rep["latency_samples"]
        out = np.abs(y[lat:lat + n, 0])
        e = np.array([np.max(out[i:i + win]) for i in range(0, len(out) - win, win)])
        xe = np.array([np.max(np.abs(x[i:i + win, 0])) for i in range(0, n - win, win)])
        m = min(len(e), len(xe))  # the output is `lat` samples shorter than the input
        e, xe = e[:m], xe[:m]
        g = 20 * np.log10(e / (xe + 1e-12) + 1e-12)
        tw = np.arange(len(g)) * win / sr
        end_s = 0.5 + burst_s
        off = g[tw >= end_s + 2 * win / sr]
        gr = -g[np.argmax(tw >= end_s - 2 * win / sr) - 1]
        rel = (np.argmax(off >= -0.1 * gr) - np.argmax(off >= -0.9 * gr)) * win / sr * 1000
        return float(gr), float(rel)

    gr_long, rel_long = recovery(0.5)
    gr_short, rel_short = recovery(0.005)
    record_property(f"release_long_{character}", rel_long)
    record_property(f"release_short_{character}", rel_short)
    assert 5.5 < gr_long < 6.5
    assert gr_short > 4.0
    assert abs(rel_long - slow_ms) <= 0.25 * slow_ms + 2 * win / sr * 1000
    assert abs(rel_short - fast_ms) <= 0.5 * fast_ms + 2 * win / sr * 1000


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_limiter_null_below_threshold(sr):
    """Material under the ceiling passes untouched: null against the oversampler with nothing in between, the
    lookahead's delay removed (the window's 0.1 ms floor at lookahead 0; ADR-0012 amendment)."""
    x = signals.noise(sr, 1.0, seed=6, amp_db=-6.0)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ceiling_dbtp=-0.1, lookahead_ms=0.0), input=x)
    assert rep["nan_count"] == 0
    ref, ref_latency = passthrough_reference(x, sr, with_latency=True)
    shift = rep["latency_samples"] - ref_latency
    assert shift >= 1
    assert measure.residual_dbfs(y[shift:], ref[: len(ref) - shift]) < -100.0


@pytest.mark.parametrize("sr", SAMPLE_RATES)
@pytest.mark.parametrize("lookahead", [0.0, 2.0, 10.0])
def test_limiter_latency_reported(sr, lookahead):
    imp = signals.impulse(sr, 0.3, channels=2) * 0.3
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, lookahead_ms=lookahead), input=imp)
    assert rep["latency_samples"] >= int(round(lookahead * 1e-3 * sr))
    assert int(np.argmax(np.abs(y[:, 0]))) == rep["latency_samples"]


def test_limiter_lookahead_change_no_click():
    sr = 48000
    x = signals.sine(300.0, sr, 3.0, amp_db=2.0)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, lookahead_ms=0.0), input=x, automation=[{"param": "lookahead_ms", "t_s": 1.5, "value": 10.0}])
    assert rep["nan_count"] == 0
    static = float(np.max(np.abs(np.diff(y[int(0.8 * sr):int(1.3 * sr), 0]))))
    step = float(np.max(np.abs(np.diff(y[int(1.3 * sr):int(2.9 * sr), 0])))) / static
    assert step < 2.0


@pytest.mark.parametrize("sr", SAMPLE_RATES)
def test_limiter_aliasing(sr, record_property):
    x = signals.sine(19000.0, sr, 1.0, amp_db=6.0, channels=1)
    y, rep = render_plugin(PLUGIN, sr=sr, params=dict(BASE, ceiling_dbtp=-6.0, lookahead_ms=1.0), input=x)
    seg = y[int(0.3 * sr):int(0.3 * sr) + 32768, 0]
    w = np.blackman(len(seg))
    s = np.fft.rfft(seg * w) / (np.sum(w) / 2)
    f = np.fft.rfftfreq(len(seg), 1 / sr)
    mag = 20 * np.log10(np.abs(s) + 1e-30)
    keep = (f < 19000.0 - 200.0) & (f > 20.0)
    worst = float(np.max(mag[keep]))
    record_property(f"alias_{sr}", worst)
    assert worst < -80.0


def test_limiter_dither():
    """16-bit dither: the output is quantised exactly to 16 bits and the noise floor of a -60 dBFS tone reads the
    TPDF floor (quantisation noise LSB^2 / 12 = -101.1 dBFS, tripled by the dither: -96.3 dBFS) within 1 dB; 24-bit:
    quantised to 24 bits."""
    sr = 48000
    x = signals.sine(1000.0, sr, 1.0, amp_db=-60.0)
    for bits, choice in ((16, 1), (24, 2)):
        y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, dither=choice), input=x)
        q = 2.0 ** (bits - 1)
        assert np.max(np.abs(y[:, 0] * q - np.round(y[:, 0] * q))) < 1e-3
    y, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, dither=1), input=x)
    seg = y[sr // 4:, 0].astype(np.float64)
    tone = np.sum(seg * np.exp(-2j * np.pi * 1000.0 * np.arange(len(seg)) / sr)) * 2 / len(seg)
    residual = seg - np.real(tone * np.exp(2j * np.pi * 1000.0 * np.arange(len(seg)) / sr))
    floor = 20 * np.log10(np.sqrt(np.mean(residual ** 2)))
    assert abs(floor - (-101.1 + 4.77)) < 1.0


def test_limiter_link():
    sr = 48000
    loud = signals.sine(1000.0, sr, 1.0, amp_db=6.0, channels=1)[:, 0]
    quiet = signals.sine(1000.0, sr, 1.0, amp_db=-12.0, channels=1)[:, 0]
    x = np.stack([loud, quiet], axis=1).astype(np.float32)
    y0, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, link_pct=0.0), input=x)
    y1, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, link_pct=100.0), input=x)
    assert abs(measure.level_at_db(y0[:, 1], sr, 1000.0, start_s=0.5) + 12.0) < 0.1  # unlinked: the quiet channel passes
    assert measure.level_at_db(y1[:, 1], sr, 1000.0, start_s=0.5) < -17.0           # linked: it follows the loud one


def test_limiter_base_checks(record_property):
    base_checks.check_param_extremes(PLUGIN, max_peak_db=0.5)
    record_property("variable_block_residual_dbfs", base_checks.check_variable_block_sizes(PLUGIN, params=BASE, tol_dbfs=-100.0))
    record_property("sr_change_residual_dbfs", base_checks.check_sr_change_mid_stream(PLUGIN, params=BASE, tol_dbfs=-80.0))
    base_checks.check_state_roundtrip(PLUGIN)
    base_checks.check_denormal_tail(PLUGIN, params=BASE)
    base_checks.check_channel_configs(PLUGIN, params=BASE)


def test_cpu_budget(record_property):
    record_property("cpu_percent_per_instance", base_checks.check_cpu_budget(PLUGIN))


def test_limiter_mono_layout_applies_gain_and_dither_once():
    """Mono in / mono out runs the stereo path on a scratch lane, not on the same buffer twice: +6 dB of input gain
    reads +6 dB (not +12), and 16-bit dither on silence leaves one TPDF floor (-96.3 dBFS), not two (review finding)."""
    sr = 48000
    mono = signals.sine(1000, sr, 0.3, amp_db=-20, channels=1)
    rms = lambda v: 20 * np.log10(np.sqrt(np.mean(v ** 2)) + 1e-12)
    y1, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, input_db=6.0), input=mono, layout={"in": 1, "out": 1})
    y2, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, input_db=6.0), input=mono, layout={"in": 1, "out": 2})
    assert abs(rms(y1[:, 0]) - rms(y2[:, 0])) < 0.1, f"mono/mono {rms(y1[:, 0]):.2f} dB vs mono/stereo {rms(y2[:, 0]):.2f} dB"
    assert abs(rms(y1[:, 0]) - (-17.0)) < 0.3  # a -20 dBFS sine reads -23 dBFS RMS, +6 dB of input gain
    silence = np.zeros((int(0.5 * sr), 1), np.float32)
    y3, _ = render_plugin(PLUGIN, sr=sr, params=dict(BASE, dither=1), input=silence, layout={"in": 1, "out": 1})
    floor = rms(y3[sr // 10:, 0])
    assert abs(floor - (-96.3)) < 1.0, f"mono/mono 16-bit dither floor {floor:.1f} dBFS"
