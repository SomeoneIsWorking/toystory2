---
id: 27
title: Toy Story 2 FMV overlay owns a blocking guest movie loop
status: investigating
symptom: FMV/FMV.BIN function 0x800D7088 decodes and presents every STR frame inside one guest call and directly calls linked VSync 0x80088628 once per movie frame
state_items: S003,S007
tags: frame-loop,vsync,fmv,re18
created: 2026-08-27
updated: 2026-08-27
---

## Root cause


## What was tried / dead ends


## Resolution

### Note (2026-08-27)
Ghidra decompilation and exact retail instructions show that 0x800D7088 performs the entire STR open/demux/MDEC/upload/display loop and calls VSync at return PC 0x800D7590 once per movie frame. Its only direct caller is 0x800D6628. psxport `Fmv::play` is a native blocking movie owner, but it returns frame count and does not expose the guest contract's playback-mode skip result; do not substitute it until the title seam preserves that outcome.

### Live boundary (2026-09-12)

After the exact LEVEL00 RAW transaction returned through bounded Lightrec execution, the next strict
front-end call reached FMV entry `0x800D6628` and refused after 46 cycles: no active code-image identity
covered that address. The title now has a shared-slot observer that authenticates the FMV source and
transferred bytes, retires MEMORY, invalidates stale translations, and publishes an FMV generation.
That owner passes synthetic identity transitions and a bounded retail run published authenticated
FMV generation 4. The run next logged `CdRead(1 sectors) with NO Setloc` and exhausted a strict guest
call at `0x800940F4`; their relationship is not yet established. This precedes the independent
movie-loop ownership above; the run completed no frame.

### CD result path (2026-09-12, static and bounded live)

The first FMV `CdRead(1)` call at `0x800D879C` follows `FUN_80090850(1, 0x800EB758)`.
That boot function builds a track table by sending GetTN (`0x13`) and GetTD (`0x14`) through
`FUN_80090FA4`, which directly calls the stock command entry `0x80091DE4` and then waits at
`0x80091898` with the same result pointer. The FMV caller takes the first track's returned
BCD minute/second bytes from `0x800EB75D/E`, computes its sector base, adds `0x10`, converts
that sector to MSF in `0x800D8594`, and calls `0x80090E78(0x15, MSF)`. The latter sends
Setloc (`0x02`) before SeekL (`0x15`) when its non-null position pointer is supplied.

The configured native `cd_command_stock_sync` and `cd_sync_stock_sync` both zero their result
buffers; neither implements GetTN/GetTD output. A bounded debugger run against the existing
Clang binary (SHA-256 `2ec3355455ca3f24a0efa2e1d9c3f5e4344e974e5b15036ca5a427f11b039461`,
build receipt psxport `8b210329`) reached authenticated FMV generation 4 and observed one
GetTN plus two GetTD commands, all returning `00 00 00 00`. The guest then **did** send Setloc
with `00 00 16 01` (BCD MSF `00:00:16`), which changed `Cd::setloc_lba` from 1141 to -1;
SeekL followed, and the exact FMV `CdRead(1, mode 0x80)` caller `0x800D87A4` saw LBA -1.
The stop was the seventh stock read after six earlier reads with valid Setloc positions;
30 native commands had included seven Setloc, seven SeekL, one GetTN, and two GetTD calls.
The same probe therefore demonstrated both outcomes. The “NO Setloc” diagnostic conflates
invalid position with absent command.

The missing TOC command/completion result lifecycle was the CD refusal's root cause. Shared
psxport now derives GetTN/GetTD from `DiscState` track metadata and retains that result through
CdSync; filling only the command result would have been erased by the old sync handler.

A single bounded authentic-disc run of the Toy Story 2 Clang binary (SHA-256
`e985970d8fe355271d2ac437d09c878155caaec7e6dea3570b3cdce3cd81d406`, build receipt
psxport `9c7dd098`) reached authenticated FMV generation 4. Command **and CdSync** returned
`02 01 01 00` for GetTN, `02 59 51 00` for GetTD(0) lead-out, and `02 00 02 00` for GetTD(1).
The guest sent Setloc `00 02 16 01`, changing LBA 1141 to 16. The first FMV `CdRead(1)` at
return PC `0x800D87A4` returned `v0=1` and advanced LBA 16 to 17. Two further FMV read entries
were reached (`0x800D886C`, `0x800D8C0C`), and the guest switched display to 24-bit.
The trace counted 9 native reads (6 pre-FMV, 3 FMV), 10 Setloc, 10 SeekL, 1 GetTN, 2 GetTD,
3 TOC CdSync, and 39 native commands. It then stopped at a distinct strict frame-driver
budget exhaustion, PC `0x80094158` after 564,492 cycles. That stop's cause is unclassified;
no frame completed and no whole-run fallback or movie-playback claim follows from this run.
