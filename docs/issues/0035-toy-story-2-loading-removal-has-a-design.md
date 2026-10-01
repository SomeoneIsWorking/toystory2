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

The guest's own loader owned every CD file read, including the whole-file read primitive at
`0x80082608`, which issued `CdRead` and spun on `CdReadySync(0)`. There are no boot logos to skip.

## What has landed

1. **`ts2::cd::FileTransfer`** — a native override of `0x80082608` that reads from the disc image the
   title already has authenticated (the same owner `game/overlay/` uses) and returns the exact
   `CdlFILE` size the guest expects. This removed the blocking whole-file wait: the same 1000-field
   boot route measures 23.5 s against the guest loader's 30.2 s, and the picture at field 900 is
   byte-identical (502,426 non-black pixels). It refuses rather than transferring on a path the disc
   does not have, a destination outside guest RAM, a file that will not fit, or a sector that cannot be
   read, and `tests/toystory2_cd_hle_boundary.cpp` asserts the refusals leave the destination untouched.
2. **Bounded loops, at the two owners that contain them** — `0x80082728`'s two unbounded retry loops
   are now a bounded, refusing retry policy (`0x80082728`, 3 attempts, the guest's failure value when
   none succeeds), and the `0x80082648` search spin is gone with the read itself, so all three spin
   sites named here are native. `0x8007F174` is still the guest's, in the file *processor* rather than
   the loader.
3. **The recovered skips are now reachable and proven** — the front-end movies return at 194, 41, 73
   and 69 display fields on the standard route (a full movie is thousands), each one the guest's own
   Start/Cross cancel ending the call, and the Level-1 "PRESS X" card's Cross at `0x8007C448` takes
   the route into the level.

## Next step

Re-derive the 51 guest words `prepareResident` and `ResidentPreparation::finish` write — including
`kElapsedFields`, `kExitCountdown`, `kTransitionFlags` and `[0x800A1370]` — because they are exactly
the phase/timer words the loading rule forbids, and until they are, neither the `0x8007F174` bound nor
a payload/terminal-state comparison against retail has the evidence it needs.