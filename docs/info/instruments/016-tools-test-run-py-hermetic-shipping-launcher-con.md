---
id: I016
kind: instrument
status: trusted
created: 2026-08-24
---

## Instrument

tools/test_run.py — hermetic shipping-launcher contract and refusal harness

## Validated by

On 2026-09-04, `uv run --frozen python tools/test_run.py` passed 11/11 against the production
`tools/run.py` seam. The positive control reaches locked framework sync, product-only CMake
configure/build under `build/player`, and the native/dynarec executable sink. Forced missing cmake,
glslc, SDL3_image, FreeType and configure cases produce non-zero named refusals. The explicit-disc
case proves runtime configuration receives the supplied path without invoking an offline generator.
The command log independently proves no CTest/test target and no compiler identity restriction.

## Known failure modes

(none recorded yet)
