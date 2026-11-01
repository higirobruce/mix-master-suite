#!/usr/bin/env python3
"""Generate Params.h and tool_schema.json from a plugin's params.json (ADR-0002).

Usage: gen_params.py <params.json> --out <dir> [--check]
  --check  exit 1 if the outputs on disk differ from what would be generated (nothing written).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

SCHEMA_VERSION = "mm-schema-1"
ID_RE = re.compile(r"^[a-z][a-z0-9_]*$")
ACCENT_RE = re.compile(r"^#[0-9A-Fa-f]{6}$")
FAMILIES = {"eq", "dynamics", "time_space", "stereo", "mastering", "utility"}
TYPES = {"float", "int", "bool", "choice"}
RESERVED = [
    {"id": "bypass", "name": "Bypass", "type": "bool", "default": False, "ai": False, "group": "header"},
    {"id": "trim_db", "name": "Output trim", "type": "float", "unit": "dB", "min": -12, "max": 12, "default": 0,
     "skew": 1.0, "smoothing_ms": 20, "ai": False, "group": "footer"},
]


class ParamSpecError(ValueError):
    pass


def _num(v, what: str) -> float:
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        raise ParamSpecError(f"{what} must be a number, got {v!r}")
    return float(v)


def _validate_param(p: dict, seen: set[str]) -> dict:
    if not isinstance(p, dict):
        raise ParamSpecError("each param must be an object")
    pid = p.get("id")
    if not isinstance(pid, str) or not ID_RE.match(pid):
        raise ParamSpecError(f"invalid identifier {pid!r}: must match {ID_RE.pattern}")
    if pid in {r["id"] for r in RESERVED}:
        raise ParamSpecError(f"{pid!r} is reserved and added automatically")
    if pid in seen:
        raise ParamSpecError(f"duplicate parameter id {pid!r}")
    seen.add(pid)
    if not isinstance(p.get("name"), str) or not p["name"]:
        raise ParamSpecError(f"{pid}: name is required")
    t = p.get("type")
    if t not in TYPES:
        raise ParamSpecError(f"{pid}: type must be one of {sorted(TYPES)}")
    out = {"id": pid, "name": p["name"], "type": t, "group": p.get("group", "main"), "ai": bool(p.get("ai", True))}
    if not isinstance(out["group"], str) or not out["group"]:
        raise ParamSpecError(f"{pid}: group must be a non-empty string")
    if t in ("float", "int"):
        for key in ("unit", "min", "max", "default", "skew"):
            if key not in p:
                raise ParamSpecError(f"{pid}: {key} is required for {t} parameters")
        if not isinstance(p["unit"], str):
            raise ParamSpecError(f"{pid}: unit must be a string")
        lo, hi, de, sk = (_num(p[k], f"{pid}.{k}") for k in ("min", "max", "default", "skew"))
        if not lo < hi:
            raise ParamSpecError(f"{pid}: range invalid, min must be < max")
        if not lo <= de <= hi:
            raise ParamSpecError(f"{pid}: default {de} outside range [{lo}, {hi}]")
        if sk <= 0:
            raise ParamSpecError(f"{pid}: skew must be > 0")
        sm = _num(p.get("smoothing_ms", 0 if t == "int" else None), f"{pid}.smoothing_ms") if (
            "smoothing_ms" in p or t == "int") else None
        if sm is None:
            raise ParamSpecError(f"{pid}: smoothing_ms is required for float parameters")
        if sm < 0:
            raise ParamSpecError(f"{pid}: smoothing_ms must be >= 0")
        if t == "int":
            for k, v in (("min", lo), ("max", hi), ("default", de)):
                if v != int(v):
                    raise ParamSpecError(f"{pid}: {k} must be an integer for int parameters")
        out.update(unit=p["unit"], min=lo, max=hi, default=de, skew=sk, smoothing_ms=sm)
    elif t == "bool":
        if not isinstance(p.get("default"), bool):
            raise ParamSpecError(f"{pid}: bool default must be true/false")
        out["default"] = p["default"]
    else:  # choice
        ch = p.get("choices")
        if not isinstance(ch, list) or len(ch) < 2 or not all(isinstance(c, str) and c for c in ch):
            raise ParamSpecError(f"{pid}: choice parameters need at least 2 string choices")
        de = p.get("default")
        if isinstance(de, bool) or not isinstance(de, int) or not 0 <= de < len(ch):
            raise ParamSpecError(f"{pid}: choice default must be an index in [0, {len(ch)})")
        out.update(choices=list(ch), default=de)
    return out


def load_params(path: str | Path) -> dict:
    """Load, validate and normalise a params.json. Reserved params are appended."""
    try:
        raw = json.loads(Path(path).read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        raise ParamSpecError(f"{path}: not valid JSON: {e}") from e
    if not isinstance(raw, dict) or "plugin" not in raw or "params" not in raw:
        raise ParamSpecError("params.json needs 'plugin' and 'params'")
    pl = raw["plugin"]
    if not isinstance(pl, dict):
        raise ParamSpecError("plugin block must be an object")
    for key in ("id", "name", "family", "version", "code", "accent"):
        if not isinstance(pl.get(key), str) or not pl[key]:
            raise ParamSpecError(f"plugin.{key} is required")
    if not ID_RE.match(pl["id"]):
        raise ParamSpecError(f"plugin.id invalid identifier {pl['id']!r}")
    if pl["family"] not in FAMILIES:
        raise ParamSpecError(f"plugin.family must be one of {sorted(FAMILIES)}")
    if len(pl["code"]) != 4:
        raise ParamSpecError("plugin.code must be exactly 4 characters")
    if not ACCENT_RE.match(pl["accent"]):
        raise ParamSpecError("plugin.accent must be a #RRGGBB colour")
    if not isinstance(raw["params"], list):
        raise ParamSpecError("params must be a list")
    seen: set[str] = set()
    params = [_validate_param(p, seen) for p in raw["params"]]
    params += [dict(r) for r in RESERVED]
    return {"plugin": {k: pl[k] for k in ("id", "name", "family", "version", "code", "accent")}, "params": params}


def _cstr(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _f(v: float) -> str:
    return f"{float(v)!r}f"


def render_header(spec: dict) -> str:
    pl = spec["plugin"]
    params = spec["params"]
    lines = [
        "// GENERATED by tools/gen_params.py from params.json. Do not edit.",
        "#pragma once",
        "#include <array>",
        '#include "plugin_base/ParamTable.h"',
        "",
        f"namespace mm::plugins::{pl['id']}",
        "{",
        f"inline constexpr const char* kPluginId = {_cstr(pl['id'])};",
        f"inline constexpr const char* kPluginName = {_cstr(pl['name'])};",
        f"inline constexpr const char* kFamily = {_cstr(pl['family'])};",
        f"inline constexpr const char* kVersion = {_cstr(pl['version'])};",
        f"inline constexpr const char* kPluginCode = {_cstr(pl['code'])};",
        f"inline constexpr const char* kAccent = {_cstr(pl['accent'])};",
        f"inline constexpr int kNumParams = {len(params)};",
        "",
        "namespace id",
        "{",
    ]
    lines += [f"inline constexpr const char* {p['id']} = {_cstr(p['id'])};" for p in params]
    lines += ["} // namespace id", ""]
    for p in params:
        if p["type"] == "choice":
            lines.append(f"inline constexpr const char* const k_{p['id']}_choices[] = {{"
                         + ", ".join(_cstr(c) for c in p["choices"]) + "};")
    lines += ["", f"inline constexpr std::array<mm::ParamSpec, kNumParams> kParamSpecs = {{{{"]
    for p in params:
        t = {"float": "Float", "int": "Int", "bool": "Bool", "choice": "Choice"}[p["type"]]
        unit = _cstr(p.get("unit", ""))
        if p["type"] in ("float", "int"):
            lo, hi, de, sk, sm = (_f(p[k]) for k in ("min", "max", "default", "skew", "smoothing_ms"))
        elif p["type"] == "bool":
            lo, hi, de, sk, sm = "0.0f", "1.0f", ("1.0f" if p["default"] else "0.0f"), "1.0f", "0.0f"
        else:
            lo, hi, de, sk, sm = "0.0f", _f(len(p["choices"]) - 1), _f(p["default"]), "1.0f", "0.0f"
        nchoices = len(p["choices"]) if p["type"] == "choice" else 0
        choices = f"k_{p['id']}_choices" if p["type"] == "choice" else "nullptr"
        ai = "true" if p["ai"] else "false"
        lines.append(f"    mm::ParamSpec{{{_cstr(p['id'])}, {_cstr(p['name'])}, mm::ParamType::{t}, {unit}, "
                     f"{lo}, {hi}, {de}, {sk}, {sm}, {nchoices}, {choices}, {_cstr(p['group'])}, {ai}}},")
    lines += [
        "}};",
        "",
        "inline const mm::ParamTable& table()",
        "{",
        "    static const mm::ParamTable t(kParamSpecs);",
        "    return t;",
        "}",
        f"}} // namespace mm::plugins::{pl['id']}",
        "",
    ]
    return "\n".join(lines)


def render_schema(spec: dict) -> dict:
    props: dict[str, dict] = {}
    for p in spec["params"]:
        if not p["ai"]:
            continue
        if p["type"] in ("float", "int"):
            props[p["id"]] = {"type": "number" if p["type"] == "float" else "integer",
                              "minimum": _plain(p["min"]), "maximum": _plain(p["max"]),
                              "unit": p["unit"], "description": p["name"]}
        elif p["type"] == "bool":
            props[p["id"]] = {"type": "boolean", "description": p["name"]}
        else:
            props[p["id"]] = {"type": "string", "enum": list(p["choices"]), "description": p["name"]}
    return {"schema_version": SCHEMA_VERSION, "plugin": spec["plugin"]["id"], "type": "object",
            "properties": props, "additionalProperties": False}


def _plain(v: float):
    return int(v) if float(v).is_integer() else v


def generate(src: Path, out: Path, check: bool) -> int:
    spec = load_params(src)
    outputs = {out / "Params.h": render_header(spec),
               out / "tool_schema.json": json.dumps(render_schema(spec), indent=2) + "\n"}
    changed = []
    for path, content in outputs.items():
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            changed.append(path)
    if check:
        for p in changed:
            print(f"out of date: {p}", file=sys.stderr)
        return 1 if changed else 0
    out.mkdir(parents=True, exist_ok=True)
    for path in changed:
        path.write_text(outputs[path], encoding="utf-8")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("params_json")
    ap.add_argument("--out", required=True)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    try:
        return generate(Path(a.params_json), Path(a.out), a.check)
    except ParamSpecError as e:
        print(f"gen_params: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
