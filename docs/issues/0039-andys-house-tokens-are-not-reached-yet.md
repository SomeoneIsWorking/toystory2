# Andy's House tokens are not reached: the level's own object list drives the steering, but the path is not planned

**State**: open

**Found while**: playing Level 1 through a control-channel session and steering Buzz with the
d-pad only (`tools/drive_level.py` against `PSXPORT_DEBUG_SERVER`).

## What the player sees

Buzz walks. Holding a d-pad direction moves him about 6,000 shifted world units per second and
releasing stops him, so input is a complete control channel. The room is the problem: a straight
approach to the level's own token coordinates runs into the bed, the wardrobe or a stair. Holding one
direction long enough to outlast the guest's turn-in and sidestepping when all four directions fail
closes the first candidate from 5,533 to 1,054 units (`tools/drive_level.py`, 60 s budget), and the
walker has never raised the game's own "a token was collected" flag at `0x800A1544`. No token has
been collected, so the level's real exit has never been seen and Level 2 has never loaded.

## What is known, and from where

- The area's placed-object table is the word at `0x800A1274` → `[count][entry…]`; room 1 has 81
  entries, of which 54 are populated and exactly 5 have a non-zero `record + 0x18`, the guest's own
  existence test (`FUN_80048638`).
- The five token ids the level itself holds are at `0x800A8668`, 16-bit at stride 8:
  `0x39`, `0x7968`, `0x3A`, `0x7978`, `0x3B`. Three of them index this area's table.
- `FUN_8007678C` case 2 is the pickup: it matches an object's id against that table, then stores
  `2` to `0x800A866C + 16*slot`, ORs `1 << slot` into a collected byte, and sets `0x800A1544 = 1`.
  That is a read-only oracle for whether a pickup happened.

## Root cause of the blockage

Not a defect: the steering loop answers "which way does this button go from here", and the guest
answers by moving. It has no notion of the room, so a straight line into furniture is the best it
can do, and `FUN_8002EA98`'s height field is still unresolved (the resolved triples carry a second
coordinate of 3516/5955/4839 where Buzz's own Y is 1875), so even a perfectly walked approach may
be standing at the wrong altitude for the pickup test. A hold shorter than the guest's turn-in also
measures the turn and not the walk, which is why the walk loop holds one direction for half a
second rather than testing four of them a fifth of a second at a time.

## Next step

Give the walker the room rather than a point: treat the area's populated records as obstacles by
walking them, and resolve the height field of `FUN_8002EA98` from the instructions behind it
(Ghidra renders that routine's body at a different address than its label, so read the words). Only
after a walk raises `0x800A1544` is a `.pad` route worth cutting, because a route that never
collects anything is not the level's route.

**State**: open
