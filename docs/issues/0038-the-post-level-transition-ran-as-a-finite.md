# The post-level transition ran as a finite call and overflowed the frame capture

**State**: resolved

**Found while**: playing Andy's House to its exit through the game's own pause menu
(`START` → `DOWN`×3 → `CROSS` on `EXIT LEVEL` → `DOWN` → `CROSS` on `YES`), pad replay
`replays/toystory2_andys_house_area_transition_v1.pad`.

## What the player saw

Choosing `EXIT LEVEL` and confirming `YES` killed the run twice, in two different ways:

1. `executor:error  frame driver required a completed guest call, but execution exited as
   budget-exhausted at 0x800215B4 after 564482 cycles`. `0x800215B4` is inside `FUN_80021190`,
   the game's RLE decompressor, called only from `FUN_8003B544` (`0x8003B710`), which is the asset
   decoder both cold-load sites already documented.
2. With that bounded away, `presentation:error  FramePresenter::capture OVERFLOW: 65508 captured
   + 31 this flush > RQ_MAX 65536`, after roughly 4,500 display lists in 0.47 s with no VBlank and
   no present in between (`debug rqflush`: an endless `n=1` / `n=20` pair at `y=[256..496]`, no
   flush anywhere near `RQ_MAX` — the accumulation, not one frame's size, was the fault).

## Root cause

`0x8007BC74`, the memory dispatcher, was called with a per-turn budget at sites that load an asset
set. Every argument that names a set runs `0x8003D88C` → `0x8003B544` → `0x80021190`, a
back-reference decode that one turn cannot finish: the cold `LEVEL00/LEVEL.RAW` corpus is 160,484
bytes over seven CRC-verified chunks and 15.2M cycles, 28 host turns, and only the cold call site
used the finite-initialization bound.

The post-level site (`0x8007BC74(4, 0x40)`) was worse than a small bound. After the decode it
enters the guest's transition SCREEN, which waits on `0x8003FA68` between screens. A finite call
supplies that barrier without delivering a field (`completeOwnedFieldBarrier` only raises the
`FrameBoundary` exit when `yieldAtFieldBarrier` is set), so the guest's screen loop ran free at
about 160 screens a second, thousands of display lists into one uncaptured frame.

So one guest function had two owners: a finite transaction where it loads, and a per-frame call
where it loads and draws.

## Fix

The bound belongs to what the call does, not to the call site:

* `callMemoryDispatcher` is the finite-initialization owner for the dispatcher calls that only load
  (the cold front end, the memory dialog, the front-end restart, the sequence memory and finale
  loads). Nothing loads a set on a per-turn budget any more.
* The post-level transition (`kMemoryDispatcher(4, 0x40)`) is a FIELD-SPANNING guest call, like the
  title poll that is already driven one field per step: new `OuterLoopPhase::levelTransition` /
  `PostResidentTransition::levelTransition` and `OuterLoopBoundary::pollLevelTransitionEvent`, with
  `ResumableGuestCall` running it under `yieldAtFieldBarrier` so the guest's own barrier delivers the
  field. The two one-turn bookkeeping calls that follow it run when it returns.

Measured after the fix, 16:9 `1280x720`: `guest call level transition returned after 700 display
field(s) in 715 turn(s)` — one present per field, no overflow — then
`LEVEL01/LEVEL.BIN image 14:14` and `resident level start returned after 302 display field(s)`, and
the next area presents and animates (`scratch/play/x7/w2900.png`, `w4199.png`).

## Not claimed

Exiting the level through the pause menu is the guest's own "leave now" route; it does not complete
the level, so the sequence returns to Andy's House rather than advancing to Level 2. The token
count on the results screen stayed at zero. Reaching the level's real exit — and therefore Level 2 —
is still open.