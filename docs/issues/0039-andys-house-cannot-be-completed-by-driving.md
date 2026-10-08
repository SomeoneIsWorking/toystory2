# Andy's House cannot be completed by driving: the route is abandoned in favour of what the game itself offers

**State**: closed — abandoned, not fixed

**Found while**: playing Level 1 through a control-channel session and steering Buzz with the
d-pad only.

## Decision

Real-completion driving is abandoned. An LLM cannot beat a level: playing Toy Story 2 to the point
where the front end unlocks the next level means solving a platformer through a d-pad, and the
attempts produced a walker that was not playing the game, it was pressing buttons and watching a
number move. That is not evidence about the port.

The level walker (`tools/drive_level.py`, since deleted with its README and codemap rows) is gone
rather than parked, because its premise — steer toward a coordinate until the game's own flag moves
— is the thing being abandoned. The control channel stays: it still drives input, reads guest words
and captures frames, which is how every verified route in this repo was produced.

## What the attempts established before they were stopped

- The area's placed-object table is the word at `0x800A1274` → `[count][entry…]`; room 1 has 81
  entries, of which 54 are populated and exactly 5 have a non-zero `record + 0x18`, the guest's own
  existence test (`FUN_80048638`).
- The five token ids the level itself holds are at `0x800A8668`, 16-bit at stride 8:
  `0x39`, `0x7968`, `0x3A`, `0x7978`, `0x3B`. Three of them index this area's table.
- `FUN_8007678C` case 2 is the pickup: it matches an object's id against that table, stores `2` to
  `0x800A866C + 16*slot`, ORs `1 << slot` into a collected byte, and sets `0x800A1544 = 1`. That
  remains a read-only oracle for whether a pickup happened.
- Input itself is a complete channel: holding a d-pad direction moves Buzz about 6,000 shifted world
  units per second and releasing stops him, measured from his own `0x800B2188` words. The problem was
  never the input path; it was that nobody was playing.
- No token was ever collected. `0x800A1544` never changed, so the level's real exit was never seen
  and Level 2 never loaded.

## What replaces it

The question is no longer "how do I play this level" but "what does the GAME offer to reach a later
level without completing it". That is a question about the executable, and it is answered by reading
it: the level-select lock check, what sets what it reads, whether the front end or the pause menu
compares a sequence of button masks against a table, and what sets the selectable cursor range. Any
such route is then entered with pad input alone through a v1 replay, never by writing guest memory.

See `docs/re-frontier.md` for that work.
