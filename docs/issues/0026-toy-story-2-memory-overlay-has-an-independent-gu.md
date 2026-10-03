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
