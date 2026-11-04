"""Drive mm_render: render a plugin or core module offline and load the result."""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

from tests.harness import signals

REPO_ROOT = Path(__file__).resolve().parents[2]
SAMPLE_RATES = [44100, 48000, 96000, 192000]
CONFIGS = [os.environ.get("MM_PLUGIN_CONFIG", "Release"), "Release", "Debug", "RelWithDebInfo"]


class RenderError(RuntimeError):
    pass


def build_dir() -> Path:
    return Path(os.environ.get("MM_BUILD_DIR", REPO_ROOT / "build"))


def mm_render_path() -> Path:
    if env := os.environ.get("MM_RENDER"):
        return Path(env)
    base = build_dir() / "tools" / "render" / "mm_render_artefacts"
    for cfg in CONFIGS:
        for name in ("mm_render", "mm_render.exe"):
            p = base / cfg / name
            if p.exists():
                return p
    for name in ("mm_render", "mm_render.exe"):
        if (base / name).exists():
            return base / name
    raise RenderError(f"mm_render not found under {base}; build the project first")


def plugin_path(name: str, fmt: str = "VST3") -> Path:
    base = build_dir() / "plugins" / name / f"{name}_artefacts"
    ext = {"VST3": ".vst3", "AU": ".component", "CLAP": ".clap"}[fmt]
    for cfg in CONFIGS:
        d = base / cfg / fmt
        if d.is_dir():
            for p in d.iterdir():
                if p.suffix == ext:
                    return p
    raise RenderError(f"no {fmt} bundle for plugin '{name}' under {base}")


def param_ranges(name: str) -> dict:
    """Range info for every parameter of a plugin, from its params.json (for exact real->normalised mapping)."""
    from tests.harness.params import load_spec

    out = {}
    for p in load_spec(name)["params"]:
        if p["type"] in ("float", "int"):
            out[p["id"]] = {"type": p["type"], "min": p["min"], "max": p["max"], "skew": p["skew"]}
        elif p["type"] == "bool":
            out[p["id"]] = {"type": "bool"}
        else:
            out[p["id"]] = {"type": "choice", "num_choices": len(p["choices"])}
    return out


def render(job: dict, scratch: Path | None = None) -> tuple[np.ndarray, dict]:
    """Run one mm_render job. Returns (audio frames x channels, report dict)."""
    own_tmp = scratch is None
    tmp = Path(tempfile.mkdtemp(prefix="mm_render_")) if own_tmp else scratch
    try:
        return _render_in(job, tmp)
    finally:
        if own_tmp:
            shutil.rmtree(tmp, ignore_errors=True)  # the result is in memory; nothing stays on disk


def _render_in(job: dict, tmp: Path) -> tuple[np.ndarray, dict]:
    job = dict(job)
    inp = job.get("input")
    if isinstance(inp, np.ndarray):
        wav = tmp / "input.wav"
        signals.write_wav(wav, inp, int(job["sample_rate"]))
        job["input"] = {"file": str(wav)}
    job.setdefault("output", str(tmp / "output.wav"))
    job.setdefault("report", str(tmp / "report.json"))
    job_file = tmp / "job.json"
    job_file.write_text(json.dumps(job))
    proc = subprocess.run([str(mm_render_path()), str(job_file)], capture_output=True, text=True)
    if proc.returncode != 0:
        raise RenderError(f"mm_render exit {proc.returncode}: {proc.stderr.strip()}\njob: {json.dumps(job)[:800]}")
    audio, _ = sf.read(job["output"], dtype="float32", always_2d=True)
    report = json.loads(Path(job["report"]).read_text())
    return audio, report


def render_plugin(name: str, *, sr: int, block: int = 64, params: dict | None = None, input=None, fmt: str = "VST3",
                  **job) -> tuple[np.ndarray, dict]:
    j = {"plugin": str(plugin_path(name, fmt)), "sample_rate": sr, "block_size": block,
         "params": params or {}, "param_ranges": param_ranges(name), "input": input}
    channels = job.pop("channels", None)
    if "layout" not in job:
        n = channels or (input.shape[1] if isinstance(input, np.ndarray) else 2)
        j["layout"] = {"in": n, "out": n}
    j.update(job)
    return render(j)


def render_module(name: str, *, sr: int, block: int = 64, params: dict | None = None, input=None, channels=None, **job):
    n = channels or (input.shape[1] if isinstance(input, np.ndarray) else 2)
    j = {"module": name, "sample_rate": sr, "block_size": block, "params": params or {}, "input": input,
         "layout": {"in": n, "out": n}}
    j.update(job)
    return render(j)


def dump_params(name: str, fmt: str = "VST3") -> list[dict]:
    proc = subprocess.run([str(mm_render_path()), "--dump-params", str(plugin_path(name, fmt))], capture_output=True,
                          text=True, check=True)
    return json.loads(proc.stdout)


def bench_plugin(name: str, *, instances: int = 100, seconds: float = 10.0, sr: int = 48000, block: int = 64,
                 reps: int = 1) -> dict:
    """Hosted bench of `instances` copies of the plugin. With reps > 1 the run with the lowest cost is returned (its
    "runs" lists every run): interference from the rest of the machine only ever adds time, so the minimum of a few
    short runs is the plugin's cost, where one long run is the plugin's cost plus whatever else the host was doing."""
    return bench_min([str(mm_render_path()), "--bench", str(plugin_path(name)), "--instances", str(instances),
                      "--seconds", str(seconds), "--sample-rate", str(sr), "--block", str(block)], reps)


def bench_min(cmd: list[str], reps: int) -> dict:
    runs = []
    for _ in range(max(1, reps)):
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            raise RenderError(f"bench failed: {proc.stderr.strip()}")
        runs.append(json.loads(proc.stdout))
    best = dict(min(runs, key=lambda r: r["cpu_percent_per_instance"]))
    best["runs"] = [r["cpu_percent_per_instance"] for r in runs]
    return best
