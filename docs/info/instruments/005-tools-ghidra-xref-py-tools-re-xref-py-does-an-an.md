---
id: I005
kind: instrument
status: trusted
created: 2026-08-20
---

## Instrument

tools/ghidra_xref.py + tools/re_xref.py — does an analyzed Ghidra project contain usable guest-address references, cross-checked against an independent per-word fold?

## Validated by

Validated 2026-08-20 on a freshly imported Ghidra 12 project (`ts2boot_re00`) made from the verified
`f90c9cd6…` executable via `tools/ram_image.py`. `python3 tools/re_xref.py --project ts2boot_re00
--selftest` passed the fold's 10/10 positive/negative/fabrication controls and 5/5 cross-method
controls: both methods independently saw `0x800103EC`; neither formed three untouched-high-RAM
negatives; malformed ranges refused; the manifest bounded 148,992 real words while Ghidra had 673
defined instructions outside those spans. Before validation, the gate exposed and fixed issue #5:
Ghidra 12 had selected standalone mode and never exercised method A. The Python wrapper also refuses
an absent project and refuses when Ghidra exits without a fresh status verdict.

## Known failure modes

The independent fold only sees addresses formed by its modeled instruction pairs; indirect,
table-derived, runtime-computed, or unmodeled register flows remain invisible. Ghidra's reference DB
depends on analysis quality and can define instructions outside the placed image, so the tracked
placement manifest—not Ghidra's extent—is the scan boundary. The wrapper refuses when the project,
image, manifest, launcher, or fresh status verdict is absent.
