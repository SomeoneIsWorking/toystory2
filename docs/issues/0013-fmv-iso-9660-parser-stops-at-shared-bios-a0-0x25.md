---
id: 13
title: FMV ISO-9660 parser stops at shared BIOS A0:0x25
status: resolved
symptom: Retail headless boot enters FMV/FMV.BIN, calls A0:0x25 from ra=0x800D8E50 with a0=0x74, and aborts before the parser reaches its second path byte
tags: boot,fmv,bios,framework,recomp
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

The shared psxport BIOS libc dispatcher omitted Sony BIOS A0:0x25 (`toupper`). The retail FMV
ISO-9660 parser calls the executable's Sony leaf for every non-separator path byte and stores the
return, so the fail-fast occurred on the first lowercase `t`.

## What was tried / dead ends


## Resolution

The shared implementation landed in psxport `ad5cf802`. This repo pins that commit, and a rebuilt
bounded retail trace passes all 15 normalized inputs for `toy2fmv\acti.str`, records 62 calls from the
exact parser return site, and reaches independent renderer issue #14.

### Note (2026-08-22)
Fresh shared-HEAD build (psxport 0f808dc9) reproduces the fail-fast. Exact retail bytes: FMV 0x800D8E48 jal 0x80082E5C, delay slot advances s1, return 0x800D8E50 stores v0 as one byte. Executable leaf 0x80082E5C selects t2=0xA0 and t1=0x25. Fresh Ghidra project ts2fmv_re11 decompiles 0x800D8D9C as an ISO-9660 path parser calling that leaf for each input character. The first live input is lowercase t. A0:0x25 is shared BIOS toupper; game-local HLE is rejected. tools/verify_fmv_boundary.py proves the static chain and requires the complete 15-byte retail sequence, not merely disappearance of the fatal.

### Resolution (2026-08-22)
psxport ad5cf802 landed shared BIOS A0:0x25 toupper. Toy Story 2 pinned that commit, rebuilt with Clang, and the bounded retail gate observed the exact first 15 normalized path bytes plus 62 calls before the independent renderer boundary.
