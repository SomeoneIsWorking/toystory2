---
id: 9
title: Toy Story 2 ReadN consumes thousands of sectors before first presentation
status: resolved
symptom: Default live boot reaches stock libcd, then mode-0xA0 ReadN advances from LBA16 through more than twenty thousand sectors and watchdog reports repeated 0x80082A80/0x80091DE4 callbacks without a frame.
tags: cd,cdc,pacing,libcd,re04
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

The retail command/completion path is serviced: `0x80091310` consumes and acknowledges INT3/INT1,
updates the distinct sync/ready state bytes, invokes the ready callback, and DMA3 moves sector data.
The old shared controller model then called its next-sector announcement immediately from each BFRD
request. At Setmode `0xA0` the physical drive rate is 150 sectors/s, but the captured 23-second window
delivered 21,164 contiguous sectors (upper bound 3,451 including the opening sector). The watchdog top
frame is therefore a consequence of missing CDC drive pacing, not proof that `0x80091DE4` lacks a
result.

## Evidence

`python3 tools/verify_cd_command.py --selftest` proves the retail ABI and exercises
impossible/bounded trace answers. Reproduce the old runaway answer against psxport `692b9b20` with
`PSXPORT_DEBUG=cdc,cdcw,irq PSXPORT_NOAUDIO=1 PSXPORT_NOWINDOW=1 PSXPORT_WATCHDOG=3
PSXPORT_WATCHDOG_BOOT=20 ./run.sh`, capture it under `scratch/logs/`, then pass it to
`python3 tools/verify_cd_command.py --trace <log> --expect runaway`.

Pinned psxport `3418a79b624765614f3f198dc1e89632e1e650f0` gives the opposite live answer without changing
any Toy Story CD field. Direct and no-argument routes both service exactly 358 sectors across eleven
ReadN phases; their longest contiguous phase is 209 sectors from LBA12506. Both retain stock
controller acknowledgement and DMA3 service, return Read|Standby status `0x22` on the first INT1,
and advance beyond the old CdSync stack into the BITS/MEMORY overlay path at
`0x800D9704`/`0x800D95C4`, with no recompilation miss; the landed default watchdog sampled resident
caller `0x8003D084`. Classify either landed
capture with `python3 tools/verify_cd_command.py --trace <log> --expect bounded`.
The `cdcr` capture additionally gates the measured status with
`--expect-int1-status 0x22`.

## Ownership

The generic fix landed in psxport CDC scheduling rather than a Toy Story game-local HLE. The game is
pinned, rebuilt with Clang, and reverified on both the direct and default routes at exact framework
revision `3418a79b624765614f3f198dc1e89632e1e650f0`; the runaway issue is resolved. The later
BITS/MEMORY boundary remains RE-04 work, but it is not this symptom.
