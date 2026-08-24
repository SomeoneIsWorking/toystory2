---
id: I016
kind: instrument
status: trusted
created: 2026-08-24
---

## Instrument

tools/test_run.py — hermetic shipping-launcher contract and refusal harness

## Validated by

On 2026-08-24, `uv run --frozen python tools/test_run.py` passed 10/10 against the production
`tools/run.py` seam. The positive control reached locked sync/recomp, product-only CMake
configure/build, and the windowed executable sink; forced missing cmake, glslc, SDL3_image, FreeType,
configure, and provisioning cases all produced non-zero named refusals before downstream mutation;
the configure refusal names the exact compiler, zlib, and zstd install commands for the detected host.
The command log independently proved no ctest/test target and no compiler version/identity probe. On
2026-08-25, a real Clang configure against recorded framework `8611d756` in the isolated
`scratch/build/player` tree then resolved the locked uv Python interpreter, recorded
`BUILD_TESTING:BOOL=OFF`, produced no `CTestTestfile.cmake`, and built the real `toystory2_port`
target through 100% without executing it.

## Known failure modes

(none recorded yet)
