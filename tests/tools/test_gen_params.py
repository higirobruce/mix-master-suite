import copy
import json
import subprocess
import sys
from pathlib import Path

import pytest

from gen_params import ParamSpecError, load_params, render_header, render_schema

VALID = {
    "plugin": {"id": "demo", "name": "Demo", "family": "utility", "version": "0.1.0", "code": "Demo", "accent": "#D8DDE4"},
    "params": [
        {"id": "gain_db", "name": "Gain", "type": "float", "unit": "dB", "min": -24, "max": 24, "default": 0, "skew": 1.0, "smoothing_ms": 20},
        {"id": "mono_sum", "name": "Mono sum", "type": "bool", "default": False},
        {"id": "mode", "name": "Mode", "type": "choice", "choices": ["A", "B"], "default": 0, "ai": False},
    ],
}


def write(spec: dict, tmp_path: Path) -> Path:
    p = tmp_path / "params.json"
    p.write_text(json.dumps(spec))
    return p


def test_reserved_params_appended(tmp_path):
    spec = load_params(write(VALID, tmp_path))
    assert [p["id"] for p in spec["params"]][-2:] == ["bypass", "trim_db"]
    trim = spec["params"][-1]
    assert trim["min"] == -12 and trim["max"] == 12 and trim["unit"] == "dB" and trim["ai"] is False


def test_header_contains_ids_and_table(tmp_path):
    h = render_header(load_params(write(VALID, tmp_path)))
    assert 'inline constexpr const char* gain_db = "gain_db";' in h
    assert "kNumParams = 5" in h
    assert '"dB"' in h
    assert "namespace mm::plugins::demo" in h
    assert 'kPluginId = "demo"' in h and 'kPluginCode = "Demo"' in h
    assert "k_mode_choices" in h


def test_schema_excludes_ai_false_and_reserved(tmp_path):
    s = render_schema(load_params(write(VALID, tmp_path)))
    assert set(s["properties"]) == {"gain_db", "mono_sum"}
    assert s["properties"]["gain_db"] == {"type": "number", "minimum": -24, "maximum": 24, "unit": "dB", "description": "Gain"}
    assert s["properties"]["mono_sum"] == {"type": "boolean", "description": "Mono sum"}
    assert s["schema_version"] == "mm-schema-1"
    assert s["plugin"] == "demo"
    assert s["additionalProperties"] is False


@pytest.mark.parametrize(
    "mutate,msg",
    [
        (lambda s: s["params"][0].pop("unit"), "unit"),
        (lambda s: s["params"].append(dict(s["params"][0])), "duplicate"),
        (lambda s: s["params"][0].update(default=99), "range"),
        (lambda s: s["params"].append({"id": "bypass", "name": "x", "type": "bool", "default": False}), "reserved"),
        (lambda s: s["params"][0].update(id="Gain-dB"), "identifier"),
        (lambda s: s["params"][0].update(skew=0), "skew"),
        (lambda s: s["params"][2].update(default=5), "choice"),
        (lambda s: s["plugin"].update(code="TooLong"), "code"),
        (lambda s: s["plugin"].update(family="space"), "family"),
        (lambda s: s["params"][0].update(min=30), "range"),
    ],
)
def test_invalid_specs_fail(tmp_path, mutate, msg):
    spec = copy.deepcopy(VALID)
    mutate(spec)
    with pytest.raises(ParamSpecError) as err:
        load_params(write(spec, tmp_path))
    assert msg in str(err.value).lower()


def test_cli_writes_both_files_and_check_is_stable(tmp_path, repo_root):
    src = write(VALID, tmp_path)
    out = tmp_path / "gen"
    cli = [sys.executable, str(repo_root / "tools" / "gen_params.py"), str(src), "--out", str(out)]
    assert subprocess.run(cli, capture_output=True).returncode == 0
    assert (out / "Params.h").exists() and (out / "tool_schema.json").exists()
    json.loads((out / "tool_schema.json").read_text())
    assert subprocess.run(cli + ["--check"], capture_output=True).returncode == 0
    (out / "Params.h").write_text("stale")
    assert subprocess.run(cli + ["--check"], capture_output=True).returncode == 1
    assert subprocess.run(cli, capture_output=True).returncode == 0
    assert (out / "Params.h").read_text() != "stale"
