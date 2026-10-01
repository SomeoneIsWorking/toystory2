---
id: 35
title: Toy Story 2 loading removal has a design but no landed owner
status: open
symptom: Every file load still goes through the guest's blocking whole-file read, the two unbounded loader loops are still guest code, and no loading-only wait or skip route is removed anywhere in the product
state_items: S015
tags: loading,cd,iso9660,overlay,fmv,pad-cancel
created: 2026-10-01
updated: 2026-10-01
---

## What the product does today

The guest's own loader owns every CD file read, including the whole-file read primitive at
`0x80082608`, which issues `CdRead` and spins on `CdReadySync(0)`. Nothing in this repository
overrides it, so loading is still a wall the player waits through. There are no boot logos to skip.

## The design that is owed

1. **`ts2::cd::FileTransfer`** — a native override of `0x80082608` that reads from the disc image the
   title already has authenticated (the same owner `game/overlay/` uses) and returns the exact
   `CdlFILE` size the guest expects. This is the change that removes the blocking whole-file wait.
2. **Bounded loops** — the two unbounded spin sites in the load path (`0x80082648`, `0x80082750`
   and `0x8008276C`) become a typed refusal instead of a wait the guest never leaves, and the size
   check that keeps the third loop (`0x8007F174`, which spins on a linked `VSync(0)` behind a
   `break` once a `.vh`/`.vb` file grows) unreachable.
3. **The recovered skips, made reachable** — the intro movies already carry their cancel: **Start**
   skips unconditionally, and **Cross** or a face button skips when `[0x800A1670] != 0`. The Level-1
   "PRESS X" card's cancel is **Cross** at `0x8007C448` inside `0x8007C344`. No gate drives either
   one yet, so the routes are recovered but unproven.

## Next step

Land (1) first: `FileTransfer` is self-contained and unblocks everything else. It must be a real
override at the measured address, registered only after the resident image is authenticated, and
covered by a boundary test that a wrong path, size or destination refuses rather than transferring.

(2) and (3) follow. One prerequisite blocks any payload/terminal-state comparison against retail:
`prepareResident` and `ResidentPreparation::finish` write about 51 guest words between them,
including `kElapsedFields`, `kExitCountdown`, `kTransitionFlags` and `[0x800A1370]` — exactly the
phase/timer words the loading rule forbids — so those have to be re-derived first.