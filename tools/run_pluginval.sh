#!/usr/bin/env bash
# Run pluginval (strictness 10) and clap-validator on one plugin's built formats.
# Usage: tools/run_pluginval.sh <plugin-id> [--config Release] [--build-dir build]
# Pinned release binaries, verified by sha256 before use (ADR-0001). Exit 0 iff every validation passes.
set -euo pipefail

PLUGIN=""; CONFIG=Release; BUILD_DIR=build
while [ $# -gt 0 ]; do
  case "$1" in
    --config) CONFIG="$2"; shift 2;;
    --build-dir) BUILD_DIR="$2"; shift 2;;
    *) PLUGIN="$1"; shift;;
  esac
done
[ -n "$PLUGIN" ] || { echo "usage: $0 <plugin-id> [--config Release] [--build-dir build]" >&2; exit 2; }

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS_DIR="$ROOT/$BUILD_DIR/tools"
OUT_DIR="$ROOT/$BUILD_DIR/pluginval"
mkdir -p "$TOOLS_DIR" "$OUT_DIR"

PLUGINVAL_VERSION=v1.0.4
CLAPVAL_VERSION=0.4.1
CLAPVAL_BUILD=0.4.1-127-g152b982

case "$(uname -s)" in
  Darwin)
    OS=mac
    PV_ASSET=pluginval_macOS.zip;  PV_SHA=3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f
    CV_ASSET=clap-validator-$CLAPVAL_BUILD-macos-universal.zip; CV_SHA=bbec8cd7d18274e549d5d8c12ece3cec54be966129388dd2e742b9957f2ba9f1
    ;;
  Linux)
    OS=linux
    PV_ASSET=pluginval_Linux.zip;  PV_SHA=c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352
    CV_ASSET=clap-validator-$CLAPVAL_BUILD-ubuntu-22.04.zip; CV_SHA=49edadcfb407ea0dd946ce418300e853fbd2660fa4b0d00e4f19ff8eef24ad90
    ;;
  MINGW*|MSYS*|CYGWIN*|Windows_NT)
    OS=win
    PV_ASSET=pluginval_Windows.zip; PV_SHA=c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab
    CV_ASSET=clap-validator-$CLAPVAL_BUILD-windows.zip; CV_SHA=d935c3af0a45c3911ea2e900f4aa5d6709dac82bb485f0c4ce28648ab2cd0c10
    ;;
  *) echo "unsupported OS $(uname -s)" >&2; exit 2;;
esac

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1
  else certutil -hashfile "$1" SHA256 | sed -n 2p | tr -d ' \r'; fi
}

fetch() { # url dest sha
  if [ ! -f "$2" ]; then curl -sSL --retry 3 -o "$2" "$1"; fi
  local got; got="$(sha256 "$2")"
  if [ "$got" != "$3" ]; then echo "sha256 mismatch for $2: $got != $3" >&2; rm -f "$2"; exit 3; fi
}

# ---- pluginval
PV_DIR="$TOOLS_DIR/pluginval-$PLUGINVAL_VERSION"
if [ ! -d "$PV_DIR" ]; then
  fetch "https://github.com/Tracktion/pluginval/releases/download/$PLUGINVAL_VERSION/$PV_ASSET" "$TOOLS_DIR/$PV_ASSET" "$PV_SHA"
  mkdir -p "$PV_DIR" && (cd "$PV_DIR" && unzip -q -o "$TOOLS_DIR/$PV_ASSET")
fi
case $OS in
  mac) PLUGINVAL="$PV_DIR/pluginval.app/Contents/MacOS/pluginval";;
  linux) PLUGINVAL="$PV_DIR/pluginval"; chmod +x "$PLUGINVAL";;
  win) PLUGINVAL="$PV_DIR/pluginval.exe";;
esac

# ---- clap-validator
CV_DIR="$TOOLS_DIR/clap-validator-$CLAPVAL_VERSION"
if [ ! -d "$CV_DIR" ]; then
  fetch "https://github.com/free-audio/clap-validator/releases/download/$CLAPVAL_VERSION/$CV_ASSET" "$TOOLS_DIR/$CV_ASSET" "$CV_SHA"
  mkdir -p "$CV_DIR" && (cd "$CV_DIR" && unzip -q -o "$TOOLS_DIR/$CV_ASSET" && for t in *.tar.gz; do [ -f "$t" ] && tar xzf "$t"; done; true)
fi
CLAPVAL="$(find "$CV_DIR" -type f \( -name 'clap-validator' -o -name 'clap-validator.exe' \) | head -1)"
[ -n "$CLAPVAL" ] || { echo "clap-validator binary not found in $CV_DIR" >&2; exit 3; }
chmod +x "$CLAPVAL" 2>/dev/null || true

ART="$ROOT/$BUILD_DIR/plugins/$PLUGIN/${PLUGIN}_artefacts/$CONFIG"
[ -d "$ART" ] || ART="$ROOT/$BUILD_DIR/plugins/$PLUGIN/${PLUGIN}_artefacts"   # single-config generators
status=0
summary=()

run_pluginval() { # format path
  local fmt="$1" path="$2"
  if [ ! -e "$path" ]; then summary+=("$fmt: NOT BUILT"); status=1; return; fi
  echo "== pluginval $fmt: $path"
  if "$PLUGINVAL" --strictness-level 10 --validate-in-process --timeout-ms 600000 --output-dir "$OUT_DIR" --validate "$path"; then
    summary+=("$fmt: PASS")
  else
    summary+=("$fmt: FAIL"); status=1
  fi
}

VST3="$(ls -d "$ART"/VST3/*.vst3 2>/dev/null | head -1 || true)"
run_pluginval VST3 "${VST3:-$ART/VST3/missing.vst3}"
if [ "$OS" = mac ]; then
  # macOS only finds Audio Units installed in a Components folder: install a copy for the check, remove it after
  # (keep it with MM_KEEP_AU=1 for DAW testing).
  AU="$(ls -d "$ART"/AU/*.component 2>/dev/null | head -1 || true)"
  if [ -n "$AU" ]; then
    AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"; mkdir -p "$AU_DIR"
    AU_INSTALLED="$AU_DIR/$(basename "$AU")"
    rm -rf "$AU_INSTALLED" && cp -R "$AU" "$AU_INSTALLED"
    killall -9 AudioComponentRegistrar 2>/dev/null || true; sleep 1
    run_pluginval AU "$AU_INSTALLED"
    [ "${MM_KEEP_AU:-0}" = 1 ] || rm -rf "$AU_INSTALLED"
  else
    summary+=("AU: NOT BUILT"); status=1
  fi
fi
CLAP="$(ls -d "$ART"/CLAP/*.clap 2>/dev/null | head -1 || true)"
if [ -n "$CLAP" ]; then
  echo "== clap-validator: $CLAP"
  if "$CLAPVAL" validate "$CLAP"; then summary+=("CLAP: PASS"); else summary+=("CLAP: FAIL"); status=1; fi
else
  summary+=("CLAP: NOT BUILT"); status=1
fi

echo "---- $PLUGIN validation summary"
printf '  %s\n' "${summary[@]}"
exit $status
