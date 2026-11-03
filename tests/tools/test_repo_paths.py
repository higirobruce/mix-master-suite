"""Every tracked path checks out on Windows: no characters NTFS refuses, no trailing dot or space, no reserved device
names. A bus compressor preset named "Gentle 2:1" failed actions/checkout on windows-2022 (CI run 41715610718)."""
from __future__ import annotations

import re
import subprocess

from tests.harness.render import REPO_ROOT

RESERVED = {"CON", "PRN", "AUX", "NUL"} | {f"COM{i}" for i in range(1, 10)} | {f"LPT{i}" for i in range(1, 10)}


def test_tracked_paths_are_windows_safe():
    paths = subprocess.run(["git", "ls-files"], cwd=REPO_ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    bad = []
    for p in filter(None, paths):
        for part in p.split("/"):
            if re.search(r'[<>:"|?*\\\x00-\x1f]', part) or part.endswith((".", " ")) or part.split(".")[0].upper() in RESERVED:
                bad.append(p)
                break
    assert bad == [], f"paths Windows cannot check out: {bad}"
