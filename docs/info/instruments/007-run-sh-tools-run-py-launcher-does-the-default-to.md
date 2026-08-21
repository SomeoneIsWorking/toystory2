---
id: I007
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

`run.sh` -> `tools/run.py` launcher — does the default Toy Story 2 route resolve the framework/disc,
provision and refresh the verified substrate, build with Clang, and launch the real project target?

## Validated by

Revalidated after RE-02 on 2026-08-21 against the clean shared framework at exact pinned revision
`692b9b20e3d4a6194452522060fd2657c2235f40`. The real zero-argument route resolved that framework and
the configured disc, provisioned the identity-checked executable and exact 21-module overlay corpus,
reused the current hash-matched generated substrate, configured Clang, built `toystory2_port`, and
launched it past generic BIOS A0:0x15 into stock-libcd command/completion poll `0x80091DE4`. The
production-injected `--selftest` passed 3/3: the default command sequence
reaches the real port executable, a missing required tool is named, and provisioning failure stops
before configure/build. CMake independently refuses a non-Clang C++ compiler.

## Known failure modes

(none recorded yet)
