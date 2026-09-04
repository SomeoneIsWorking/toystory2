---
id: C012
kind: claim
status: holds
created: 2026-08-21
tags: cd,re04,cdc
depends: tools/verify_cd_command.py
---

## Claim

Toy Story 2 stock libcd command/completion ABI is instruction-verified; the resolved RE-04 runaway was the framework CDC exposing next-sector INT1 results without drive pacing, not an unserviced CdControl result

## Evidence

C012/I012: verified SLUS_008.93 instructions prove 0x80091DE4(cmd,param,result,async), pre-sync 0x80091898 and interrupt service 0x80091310. INT1 updates ready state 0x800A0AE5/callback 0x800A0808; INT2/3/5 update sync state 0x800A0AE4/callback 0x800A0804. A live mode-0xA0 ReadN at derived LBA16 acknowledges controller results and performs DMA. The old psxport `692b9b20` baseline then exposed 21,164 contiguous sectors during a conservative 23-second watchdog window; double-speed hardware permits at most 3,451 including the opening sector. Exact pinned psxport `3418a79b624765614f3f198dc1e89632e1e650f0` provides the real opposite answer with the same game configuration on both direct and default routes: 358 total sectors over eleven ReadN phases, a longest phase of 209 sectors from LBA12506, first-INT1 status 0x22, preserved acknowledgement/DMA, and a later observed MEMORY-overlay boundary with no execution fault or zero-filled DMA word.

## What would falsify it

Falsified if the retail instruction verifier no longer derives the ABI/state map, if pinned psxport `3418a79b` produces the impossible sector burst, or if the later boundary reproduces with the unpaced `692b9b20` baseline; a framework change to CDC service scheduling requires rerunning the landed live classifier.
