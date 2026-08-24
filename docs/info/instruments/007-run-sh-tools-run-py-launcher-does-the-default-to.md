---
id: I007
kind: instrument
status: DISTRUSTED
created: 2026-08-21
distrusted_on: 2026-08-24
---

## Instrument

`run.sh` -> `tools/run.py` launcher — does the default Toy Story 2 route resolve the framework/disc,
provision and refresh the verified substrate, build with Clang, and launch the real project target?

## Validated by

Revalidated after RE-04 on 2026-08-21 against the clean shared framework at exact pinned revision
`3418a79b624765614f3f198dc1e89632e1e650f0`. The real zero-argument route resolved that framework and
the configured disc, provisioned the identity-checked executable and exact 21-module overlay corpus,
reused the current hash-matched generated substrate, configured Clang, built `toystory2_port`, and
launched it past generic BIOS A0:0x15 and serviced stock-libcd into the emitted BITS/MEMORY overlay
path at `0x800D9704`/`0x800D95C4`. Its bounded trace matches the direct route exactly: 358 sectors
across eleven phases, longest 209, first-INT1 status `0x22`, stock acknowledgement/DMA present, and
no recompilation miss. The production-injected `--selftest` passed 3/3: the default command sequence
reaches the real port executable, a missing required tool is named, and provisioning failure stops
before configure/build. CMake independently refuses a non-Clang C++ compiler.

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-24

The launcher implementation changed on 2026-08-24 to a frozen-uv bootstrap, CMake-owned compiler
discovery, `BUILD_TESTING=OFF` product build, and required windowed sink. This session validated its
injected-host contract but deliberately did not run the game/window, so I007's prior real-path
validation no longer covers the current implementation.

> Every result this instrument produced is suspect until it is re-validated.
