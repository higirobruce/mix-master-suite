#!/usr/bin/env python3
"""Place the EBU loudness test set (Tech 3341 / 3342 sequences) in tests/fixtures/ebu/.

Usage: fetch_ebu_vectors.py [--check] [--zip PATH]

The set is published by the EBU at
  https://tech.ebu.ch/files/live/sites/tech/files/shared/testmaterial/ebu-loudness-test-setv05.zip
behind a Cloudflare browser challenge, so a headless download returns the challenge page (verified 2026-11-03 with and
without a browser user agent). Download the zip in a browser and pass it with --zip, or drop it into
tests/fixtures/ebu/; this script verifies its sha256 when the recorded value is known, extracts the seq-3341-* and
seq-3342-* WAV files and exits 0. With the files already present it reports them and exits 0. --check only reports.
CI runs the synthetic sequences (tests/harness/ebu.py) and skips the file-based test when the files are absent.
"""
from __future__ import annotations

import argparse
import hashlib
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures" / "ebu"
URL = "https://tech.ebu.ch/files/live/sites/tech/files/shared/testmaterial/ebu-loudness-test-setv05.zip"
ZIP_SHA256 = ""  # recorded after the first verified download (see tests/fixtures/ebu/README.md)


def present() -> list[Path]:
    return sorted(FIXTURES.glob("seq-334*.wav")) if FIXTURES.is_dir() else []


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--zip", type=Path, default=None)
    a = ap.parse_args(argv)
    files = present()
    if files and a.zip is None:
        print(f"{len(files)} EBU sequence files present in {FIXTURES}")
        return 0
    if a.check:
        print("no EBU files present; download the set in a browser and run with --zip PATH")
        print(f"  {URL}")
        return 1
    zip_path = a.zip or (FIXTURES / "ebu-loudness-test-setv05.zip")
    if not zip_path.is_file():
        print(f"no zip at {zip_path}; download it in a browser from\n  {URL}\nand run again with --zip PATH", file=sys.stderr)
        return 1
    digest = hashlib.sha256(zip_path.read_bytes()).hexdigest()
    if ZIP_SHA256 and digest != ZIP_SHA256:
        print(f"sha256 mismatch: {digest} (expected {ZIP_SHA256})", file=sys.stderr)
        return 2
    FIXTURES.mkdir(parents=True, exist_ok=True)
    count = 0
    with zipfile.ZipFile(zip_path) as z:
        for info in z.infolist():
            name = Path(info.filename).name
            if name.lower().endswith(".wav") and name.startswith("seq-334"):
                (FIXTURES / name).write_bytes(z.read(info))
                count += 1
    print(f"extracted {count} files to {FIXTURES} (zip sha256 {digest})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
