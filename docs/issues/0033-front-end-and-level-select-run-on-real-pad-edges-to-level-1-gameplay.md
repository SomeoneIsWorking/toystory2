---
id: 33
title: Toy Story 2 runs title, menu, level select and the Level 1 intro on real pad edges into gameplay
status: open
symptom: Before this change the front-end poll was a non-yielding finite transaction that timed out into the attract demo, so no menu was ever presented and no pad edge could reach it
tags: front-end,poll,selection,field-barrier,overlay-identity,invalidation,fallback
state_items: S002,S003,S004,S005
created: 2026-10-01
updated: 2026-10-01
---

## What runs now (headless, silent, unpaced, real pad edges through the control channel)

Intro movies (194, 41, 73, 69 fields, unchanged on psxport 2bb8d2f6) -> title "PRESS START" ->
Start -> main menu (START GAME / OPTIONS / MEMORY CARD / MOVIE VIEWER) -> Cross -> level select
(Andy's House, animated model) -> Cross -> story movie (69 fields) -> LEVEL01 overlay authenticated
and loaded -> "LEVEL 1: ANDY'S HOUSE, PRESS X" -> Cross -> Andy's Room with Buzz idling. Captures
were opened (`tools/headless_run.py --tap FRAME:BUTTON[:N] --shot-at ...`); the title frame is
98.4% non-black, the menu and level-select frames 99.9-100%, gameplay 95.8% (denominator 691,200
pixels of the 960x720 sink). Presented frame numbers drift from `--tap` numbers because the driver
polls the counter every 0.2 s.

## Causes found, each from guest bytes

1. **The poll is the title screen's own loop.** The `0x8007BC74(2,0)` dispatch reaches MEMORY
   `0x800D92C4`, which draws, waits on `0x8003FA68` and reads the pad every field until a selection or
   a timeout. A single finite call ran it to its 900-field attract timeout in about 16 host fields
   because the native barrier override (`0x8003FA68`) returned at once. Fix: the poll and the selection
   screen are resumable calls, and the barrier, while one is running, completes with the fields it
   asked for and exits the executor at the field boundary (`ToyStory2Context::yieldAtFieldBarrier`).
   A host-initiated guest call between fields (the deferred field service) would clobber the suspended
   call's caller-saved registers, so it is skipped while a call is suspended.
2. **The selection screen's return value was inverted.** Retail `0x8007AD8C`: a NONZERO return of
   `0x80041240` stores event -1 and re-enters the poll (the player backed out); ZERO means a level was
   chosen, then the optional transition screen `0x80073408`, then level preparation. The earlier
   extraction treated nonzero as chosen. `SelectionProgress` now carries the three outcomes.
3. **The selection screen is a two-field-paced loop** (it requests the barrier with `a0 = 2`), so its
   phase delivers two display fields per host frame like the resident update.
4. The first resident update measured 1.19 fields (674,080 cycles) and exceeded one turn; it is a
   slice-limited finite call (limit 8).

## Invalidations (38,765,035 over ~377 frames; 93.9M over 5,600 frames)

Measured per source with the new `invalidations_by_source` line on the debug `guest` channel (psxport
012d7d00 and later): over 1,200 frames 45.6M of 46.3M were `MappedStore`, ie per-store notifications.
Three real defects, fixed in psxport: `cd_read_stock_sync` copied each sector with per-byte `mem_w8`
(one invalidation per byte); the MDEC-out, CD DMA3, SPU DMA4 and ClearOTagR DMA6 drains notified per
word. They now report one range per sector or burst (`Dma`/`ModuleLoad` source). What remains
(34.8M of 35.9M in 1,200 frames, 92.4M of 93.9M in 5,600) is guest CPU stores: the RAM map carries
memory callbacks, so every translated RAM store reaches `Core::writeGuestMemory`, which notifies.
That is the framework's store path by design, now labelled `Cpu`, and not reduced here. Whether the
per-store `lightrec_invalidate` is worth skipping for a store that cannot overlap translated code is
a separate, unmeasured framework question (it needs a cost measurement, not a count).

## Open / unverified

- Gameplay was observed idle (no input after the intro card); S005 is not advanced.
- HUD and the 60 fps scope are unchecked.
- A cold-path fallback ledger by reason was not recorded for the whole run.
- The psxport commits this pin names are on the local branch `ts2-level`, not on psxport main.
