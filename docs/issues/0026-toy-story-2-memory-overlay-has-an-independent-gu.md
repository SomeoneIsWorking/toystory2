---
id: 26
title: Toy Story 2 MEMORY overlay has an independent guest-owned display loop
status: investigating
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
