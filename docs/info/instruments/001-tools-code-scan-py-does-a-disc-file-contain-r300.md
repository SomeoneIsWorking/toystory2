---
id: I001
kind: instrument
status: trusted
created: 2026-08-12
---

## Instrument

tools/code_scan.py — does a disc file contain R3000A code, or only data? (the overlay census)

## Validated by

Validated in BOTH directions on THIS GAME's own data 2026-08-12 by --selftest, real output: POSITIVE = the 64 KiB at the PS-EXE header's declared entry pc0=0x80082D60, which must read >=95% code-plausible and reads 99.2% of 16,367 windows with 20.20 jr-ra per 1k words; NEGATIVE = a media file (LEVEL01/LEVEL01.VB, a PSY-Q VAB sample bank), which must read <=2% and reads 0.1% of 127,366 windows with 0 jr-ra. Exit 0, PASS - both classes behave. --census refuses (exit 2) on a glob matching zero files, saying it scanned NOTHING and that this is not a no-code-found result; it refuses (exit 2) when the framework's exe_similarity.py is not reachable through PSXPORT_DIR, naming what it did not scan. THE FILE IS A VERBATIM COPY of megamanx4/tools/code_scan.py with identical logic, thresholds and selftest - only its opening paragraph is reworded. That is deliberate: the code/data discriminator underneath it is IMPORTED from external/psxport/tools/exe_similarity.py so there is ONE calibrated copy (recalibrated + selftested 2026-08-12 over 5215 hand-disassembled code windows), and writing a fourth detector would mean a fourth calibration and a fourth set of bugs. Note that the same unmodified code produced OPPOSITE structural answers for its two consumers (megamanx4: 0 of 138 archives hold code; TS2: 21 of 274 files are code overlays, plus one file, FMV/FMV.BIN, whose class the census left UNRESOLVED), which is the strongest evidence available that it is measuring the files rather than echoing an expectation. BLIND SPOTS printed on every run: compressed/packed code reads as data and is indistinguishable from a texture; fragments under 64 bytes are invisible; it reads files as flat blobs and does not parse containers; and it says NOTHING about where code would be loaded - that is base_fit.py's question.

## Known failure modes

(none recorded yet)
