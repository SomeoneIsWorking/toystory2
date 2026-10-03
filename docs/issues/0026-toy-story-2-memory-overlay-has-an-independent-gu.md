---
id: 26
title: Toy Story 2 MEMORY overlay has an independent guest-owned display loop
status: resolved
symptom: BITS/MEMORY.BIN function 0x800DEF6C contains an internal update loop and eleven direct calls to linked VSync 0x80088628
state_items: S003
tags: frame-loop,vsync,memory-overlay,re18
created: 2026-08-27
updated: 2026-10-03
---

## Root cause


## What was tried / dead ends


## Resolution

### Note (2026-08-27)
Retail function 0x800DEF6C contains eleven direct calls to 0x80088628: eight around pad/display teardown and reinitialization at return PCs 0x800DF0C4..0x800DF114, plus three inside its UI state loop at 0x800E0820..0x800E0830. It also calls the measured field barrier 0x8003FA68 from its internal loop. This is not the resident FrameDriver and must become its own finite native state owner; successful guest VSync is forbidden.

## Reproduction and localisation (2026-10-03) — the stall is in the overlay's READ path, not yet its UI loop

`replays/toystory2_memory_card_v1.pad` reaches the MEMORY overlay through the game's own menu: the
recorded title presses, then two Down presses in the main menu, then Cross. The phase-keyed recording
keeps the boot and menu phases of `toystory2_player_andys_house_v2.pad` and inserts those three inputs
at absolute frames 700, 730 and 1000, so it re-keys itself rather than replaying a fixed count.

**Reached.** The menu cursor moves down to MEMORY CARD and the screen was captured showing it
highlighted (`present_900.png`, 691164/691200 non-black) — the route works, and the highlight is the
game's own. Cross is accepted: the log shows `BITS/MEMORY.BIN` authenticated twice more
(`image 8:8`, then `image 9:9`, 63312 bytes each) at the shared slot 0x800D5D20, so the overlay loads.

**Then the run dies**, which is this issue's symptom reached through the product rather than in
isolation:

```
[ts2-execution] guest call front-end poll returned after 471 display field(s) in 498 turn(s)
[ts2-overlay]   authenticated BITS/MEMORY.BIN image 9:9 at 0x800D5D20, 63312 bytes
[executor:error] frame driver required a completed guest call, but execution exited as
                 budget-exhausted at 0x800215B4 after 564492 cycles: cycle budget exhausted
```

Nothing is captured after frame 900: the process aborts on the frame driver's "this call had to
return" contract.

**Where the budget actually goes — a correction to this issue's recorded location.** `0x800215B4` is not
in the overlay at all: it is 1060 bytes inside `FUN_80021190` (`0x80021190..0x800218B3`) in the MAIN
executable, an RLE/bit-unpacking loop. Its only caller is `FUN_8003B544` (`jal 0x80021190` at
0x8003B710), itself called from the streaming read path `FUN_8003D88C` (0x8003DC2C) and
`FUN_8003FB0C` (0x8003FCA0). Immediately before the abort the log repeats
`irq pending I_STAT&I_MASK=0x001/0x005; no SysEnq element claimed it ... custom exception exit
installed`, i.e. the guest is taking interrupt exceptions without progressing.

So the guest stalls **while feeding the decompressor**, before `FUN_800DEF6C` is ever entered. The
overlay's own UI loop and its eleven VSync sites are real and still unmigrated, but migrating them
would not fix this run: the overlay never gets that far. The open question moved earlier in the path —
what the streaming read hands the decompressor for this module — and until that is answered the native
loop owner cannot be exercised on the product at all, which is the same conclusion issue 25 already
recorded ("static inspection is not product evidence").

The RE that established the overlay's structure is still worth keeping, since it is what the native
owner will have to reproduce: `FUN_800DEF6C` is 2142 instructions / 92 calls, opening with a finite
24-iteration "Please wait" prologue whose each iteration is `field barrier 0x8003FA68(1)` + draw,
then teardown (eight `VSync(0)` at return PCs 0x800df0c4..0x800df11c), then a UI state loop that
consumes exactly one `0x8003FA68(1)` field barrier per iteration. That shape is what the owner has to
match: one field per host turn, which this port already knows how to express, because
`context.yieldAtFieldBarrier` makes the installed field-barrier override at 0x8003FA68 exit with
`FrameBoundary` — the same seam `stepInteractiveSelection` uses for the screen loop 0x80041240.

## Resolution (2026-10-03) — the caller is one field-spanning call; both symptoms are one defect

The earlier note concluded the streaming read was the blocking defect and the overlay's UI loop was
downstream of it. Both were half-right: they are **one call**, and the port had given it a one-turn
budget.

Ghidra, exact bytes, `FUN_800415E4` (36 instructions, 5 calls, one return) — the front-end event 3
handler, which is what `case 3:` in `outer_loop.cpp` dispatches to `checkSaveSelection()`:

```c
uVar1 = DAT_800a16a8;  DAT_800a16a8 = 0x10;  DAT_800a138c = 0xf8;
FUN_80039d9c();  FUN_8003d88c(DAT_800a16a8);  DAT_800a141c = 0;
FUN_80078c84(0x800c1608);  uVar2 = func_0x800def6c();
FUN_80078cc4(0x800c1608);  DAT_800a16a8 = uVar1;  return uVar2;
```

Two independent reasons that call cannot be finite, and both are load-bearing:

1. `FUN_8003d88c(0x10)` loads `MEMORY.BIN` plus `LEVEL06/LEVEL3.RAW` (85,860 bytes) and decodes it
   through `0x8003B544 -> 0x80021190` — the same live back-reference decoder `restartColdFrontEnd`
   already documents as unfinishable in one turn (15,245,664 cycles over 28 host turns for the cold
   160,484-byte corpus). This is not a spin in the unpacker and not a malformed RAW: a walk of every
   `LEVEL00`/`LEVEL06` RAW under the guest's own advance rule (`size` at bytes 4..7, big-endian, plus
   `0x0e`, ending on four `0xFF`) reaches its terminator in 3..18 records on all of them, and the
   decoder's per-record bit count is the byte at `+0x0e`. 564,492 cycles over 85,860 bytes is ~6.6
   cycles/byte — the correct order of magnitude for a finished back-reference decode, not an infinite
   loop. The one-turn budget simply ended it mid-record.
2. `func_0x800def6c()` **is** the MEMORY overlay's own display loop: a 24-iteration "Please wait"
   prologue, then a UI state loop consuming exactly one `0x8003FA68(1)` field barrier per iteration,
   returning only when the player backs out.

**Fix.** `checkSaveSelection()` is now a resumable field call driven one display field per host turn
with `context.yieldAtFieldBarrier = true` — the same seam and the same ownership contract
`stepInteractiveSelection()` already uses for the screen loop at `0x80041240`. Nothing in the guest's
body is reimplemented: the load, the decode, the overlay's teardown and its UI loop all still run as
the guest's own code through the seam. The change only decides where the turn is handed back.
`OuterLoopBoundary::checkSaveSelection()` now returns `bool` (pending/returned), and `case 3:` stays
in `OuterLoopPhase::pollFrontEnd` until it returns, so the poll's completion is not published while the
overlay is still drawing.

**Evidence.** `replays/toystory2_memory_card_exit_v1.pad` (the recorded title presses, two Downs,
Cross at frame 1000, then Down/Cross on EXIT) at `--frames 2400`:

- No abort. The previous `budget-exhausted at 0x800215B4` failure is gone.
- The MEMORY screen is captured and **live**: `present_1200.png`, 691200/691200 non-black, Rex holding
  the memory card with the `SAVE`/`EXIT` menu and `MEMORY CARD SLOT 1`. Across frames 1200/1500/1650/
  1800/2100/2350 Rex's pose differs every frame, which is the overlay's own per-field animation — the
  loop is executing, not a single held frame.
- Run ends clean: `run-end: fallback: calls=0 instructions=0 refused_calls=0 compilation_failed=0
  self_modifying_code=0 unsupported_block=0 load_delay_hazard=0 unsafe_instruction_fetch=0 …`, over
  `executed_instructions=49387577` with `fallback_blocks=0`. No interpreter fallback anywhere on the
  route.
- `tools/verify.py` PASS.

### Not closed by this change

The run does **not** return to the title from the MEMORY screen, so "back to the title" is still
unproven. The cause is in the recording, not the guest: `run-end: replay TRUNCATED ... segments entered
2 of 5, frames delivered 997 of 2430`. Entering the overlay does not move any of the four input words
the replay is keyed to, so the replay stayed waiting on a phase transition that never came and the
Down/Cross aimed at `EXIT` were never delivered. Closing the return-to-title leg needs either the word
the overlay publishes for its own screen, or a segment keyed to the phase actually live there.

Event 4, `loadSaveSelection()` at `0x8004171C`, still has the same load-plus-loop shape
(`FUN_8003d88c` inside a `while (true)` over `func_0x800d6628`/`func_0x800dc67c`) and is still driven
by a one-turn `callGuest`. It is not yet migrated.

## Correction and follow-up (2026-10-03) — the landed fix advanced the overlay under the WRONG phase

The version landed as `58fb485` got the screen on screen but was wrong in a way the first evidence
could not see: `case 3:` called `checkSaveSelection()` and then FELL THROUGH to `finishFrontEndPoll()`
on the same step, leaving `state.phase` at `pollFrontEnd`. The next step therefore called
`pollFrontEndEvent()` again, which found the driver's single resumable `fieldCall_` still active and
advanced the MEMORY overlay's guest call while the loop believed it was polling the front end. The
overlay animated because it was being driven — by the wrong owner, under the wrong phase. When that
call returned, its return value would have been read as the front-end poll's event.

**Fix.** `OuterLoopPhase::memoryScreen` is now a phase of its own. `case 3:` calls
`beginMemorySelection()` and moves to it; `case 4:` calls `beginLoadSaveSelection()` and moves to it;
the new case steps the screen one display field at a time and only calls `finishFrontEndPoll()` and
returns to `pollFrontEnd` once the call has actually returned.
`native_frame_driver_boundary`'s `outer_loop_front_end_events_are_finite_and_non_fallthrough` now
records that contract: for events 3 and 4 the poll is NOT finished on the dispatch step, and it is
finished only after the screen returns.

**A second defect found while fixing the first.** The overlay state was initially kept in a
`memorySelection_` member of `CoreResidentFrameBoundary`. That boundary is constructed afresh on
every step and holds only REFERENCES to state that must outlive it (`fieldCall_`, `introMovieStep_`,
`selectionCall_`), so the member was silently reset to its default every single step. Measured: a
probe counter in the same class read `0` on all 207 calls. The overlay's identity now rides the
existing runtime-owned `SelectionCall` enum, extended with `memoryCardOverlay` and `loadSaveOverlay`.

### The phase was never the problem, and a fifth key word would have been wrong

The recorded cause of the replay stalling was a phase collision between the MEMORY screen and the
level-select screen (`levelId = 0x10` on both). Measured on the product, the overlay's actual phase is
`pb=0 fe=1 sel=0 level=0x10` -> **`0x20010`**, while level select is `sel=1` -> `0x30010`. They were
never equal: the MEMORY screen publishes `sel=0`, which is what separates it. The four existing words
already key this screen uniquely and no change to `toystory2_input_phase` was needed.

A fifth word was investigated and rejected on measurement. `0x800A138C` (gp+0x6B4) looks ideal: four
writers, zero readers, `FUN_800415E4` stores `0xF8`, `FUN_8004171C` stores `0xB8`, the dispatcher
stores `a1 + 0x43E`, and MEMORY.BIN contains no `lui 0x800a` and no `0x6B4` displacement so the
overlay cannot reach it. But the word reads **0** for the whole overlay, with `gp = 0x800A0CD8`
verified correct at the same sample — the overlay's own initialisation (`do { *puVar6 = 0; } while
(uVar19 < 0x21c9)` over a buffer at `_DAT_800a124c + 0x2000`) clears it. An unstable word is worse
than no word, so the change was reverted.

The replay stall was the phase ORDER: the recorded file still carried the v2 route's `0x30010` and
`0x30000` segments, which this route never visits, so the matcher waited for a phase that never came
and never advanced to the overlay's segment. `replays/toystory2_memory_card_exit_v1.pad` is rebuilt
with only the three phases the route actually visits.

### Event 4, and the evidence

`FUN_8004171C` is not a save/load screen — its body is `while (true) { FUN_8003d88c(...);
iVar2 = func_0x800dc67c(iVar2); if (iVar2 < 0) break; FUN_80082508("FMV/FMV.BIN", 0x800d5d20);
func_0x800d6628(iVar2 + 10); }`, i.e. the MOVIE VIEWER, reached from the main menu. It is migrated the
same way and measured through the product via `replays/toystory2_movie_viewer_v1.pad`:

- `front-end poll returned event 4`, all 3 segments entered, no budget abort.
- Screen captured: the binocular-car carousel with `SELECT` / `BACK`.

Both overlay routes now end clean:
`run-end: replay COMPLETE — segments entered 3 of 3` and
`run-end: fallback: calls=0 instructions=0 refused_calls=0 compilation_failed=0 self_modifying_code=0
unsupported_block=0 load_delay_hazard=0 unsafe_instruction_fetch=0 …`.

`tools/verify.py` passes 7/7 with `clang-format` clean.
