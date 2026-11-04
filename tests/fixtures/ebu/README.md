# EBU loudness test set (Tech 3341 / 3342)

The WAV files are not in the repository (about 100 MB, and the EBU serves them from behind a Cloudflare browser
challenge, so no headless fetch works — verified 2026-11-03). Get them with:

1. Download https://tech.ebu.ch/files/live/sites/tech/files/shared/testmaterial/ebu-loudness-test-setv05.zip in a browser.
2. `python tools/fetch_ebu_vectors.py --zip ~/Downloads/ebu-loudness-test-setv05.zip` — extracts `seq-3341-*.wav` and `seq-3342-*.wav` here.

`tests/plugins/test_ebu_vectors.py::test_ebu_real_vectors` then runs every file through the core meters (via
`mm_render`'s `meter: "loudness"` report) against the readings in `tests/harness/ebu.py::REAL_EXPECTED`, taken from
EBU Tech 3341 v3 Tables 1–2 and Tech 3342 Table 1: integrated loudness ±0.1 LU, momentary / short-term maxima
±0.1 LU, LRA ±1 LU, true-peak ±0.3 dBTP. Without the files the test is skipped with that message; CI relies on the
synthetic sequences in `tests/harness/ebu.py`, which follow the documents' signal definitions exactly (stereo 1 kHz
sines at the stated levels and durations) and run at 44.1 / 48 / 96 / 192 k.

The zip's sha256 is recorded in `tools/fetch_ebu_vectors.py` once a download has been verified; a mismatch stops the
extraction. Multichannel sequences (seq-3341-6, 5.1) are skipped: the M5 meters are measured in stereo.
