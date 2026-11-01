# ADR-0001: Build system, formats, dependencies, CI

**Status:** Accepted (2026-11-01)

## Context

Sixteen plugins share one C++20/JUCE 8 codebase and must ship as VST3, AU, AAX and CLAP on macOS, Windows and Linux (spec: Shared requirements). CLAUDE.md requires exact dependency pins fetched by CMake with a hash, nothing fetched at runtime, sanitizer runs in debug, and a three-OS CI matrix that runs the measurement suite and pluginval on every push. The development machine has Command Line Tools only (no Xcode) and no AAX SDK.

## Decision

- CMake ≥ 3.25 with the Ninja generator; one root `CMakeLists.txt`, helpers in `cmake/`.
- Dependencies are declared once in `cmake/Dependencies.cmake` with `FetchContent` and a full commit SHA as `GIT_TAG` (a SHA is content-addressed, so it is the hash pin CLAUDE.md asks for): JUCE 8.0.x, free-audio/clap-juce-extensions (with its clap and clap-helpers submodules), Catch2 v3. Pinned SHAs are recorded in that file; updating one is a reviewed commit.
- Formats: VST3, CLAP and Standalone everywhere; AU on macOS. AAX is built only when `MM_AAX_SDK_PATH` points at the SDK; otherwise the configure step logs "AAX stubbed" and the format is skipped. The gap is listed in every REVIEW.md until the SDK is available.
- macOS: Universal binary (`arm64;x86_64`), deployment target 12.0, built with Command Line Tools; `auval` and pluginval validate AU. Windows: MSVC 2022 x64. Linux: GCC 12, x86-64.
- Warnings as errors on our own targets only: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` (Clang/GCC), `/W4 /WX` (MSVC). JUCE and other fetched code keep their own flags.
- `MM_SANITIZE=ON` adds `-fsanitize=address,undefined -fno-omit-frame-pointer` (Clang/GCC) or `/fsanitize=address` (MSVC) to Debug builds of our targets. CI runs a sanitizer job on macOS and Linux; a sanitizer report fails the milestone.
- CI (GitHub Actions): matrix macos-14 / windows-2022 / ubuntu-22.04, each running configure, build, ctest, the Python measurement suite, pluginval strictness 10, clap-validator and the CPU benchmark against `tools/budgets.json`. Linux runs GUI-touching steps under `xvfb-run`.
- Python 3.11 is a build-time dependency (parameter code generation, `tools/gen_params.py`) and a test dependency (`tests/requirements.txt`, exact pins). `uv` creates `.venv`; CMake prefers `.venv/bin/python` and falls back to a system Python 3.11.
- pluginval and clap-validator are pinned release binaries downloaded by `tools/run_pluginval.sh` and verified by sha256 before use. They are development tools; nothing is fetched at plugin runtime and the plugins make no network calls.
- Branching: `milestone/<name>` branches, conventional commits, merge to `main` only with CI green.

## Consequences

- One place to bump a dependency; reproducible builds on all three OSes from a clean checkout plus Python 3.11.
- AAX cannot be validated until an SDK is provided; the plugin code stays format-agnostic through JUCE so enabling it later is a configure flag.
- Building Universal binaries doubles macOS compile time; acceptable for release builds, and local development can set `CMAKE_OSX_ARCHITECTURES` to the host arch.
- Warnings-as-errors with `-Wconversion` costs explicit casts in DSP code; it also catches the float/double mixing that the 64-bit filter paths depend on.
