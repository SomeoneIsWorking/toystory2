id: 44
state: fixed
title: An absolute-frame pad route no longer reaches the room it names

## The defect

`tools/verify_route.py --route` replayed `tools/ts2_route.py`'s named `andys-room` route: four
exact-frame taps (`start` at pad frame 500, `cross` at 560 and 620, `cross` at 790) judged by Buzz's
object at `0x800B2188` in the RAM dump at pad frame 900. It failed on every run:

```
[route] pad frame 900: Buzz x=0 y=0 z=0 yaw=0
[route] FAIL Buzz's object (0x800B2188) is all zero: gameplay was not entered
```

`--negative` passed for the wrong reason (it asserted Buzz is absent), and `--determinism` compared
two runs that never entered gameplay.

## Root cause

The route's frames assume how many pad frames boot, the front end and the intro movies consume. Those
frames were measured before the FMV movies were natively driven: one movie frame per host turn
(issue 0027) means the four intro movies no longer finish by pad frame 440. Every tap now lands
before the screen it was written for exists — the same root cause as issue 0043, which was resolved by
deleting `tools/verify_movement.py`, the other consumer of this route.

An exact-frame schedule is a live probe, not a recording: it is only correct while the timing it was
measured against still holds. Anything that must keep working over time has to be a phase-keyed
recording, whose presses are offsets from the screen each was captured on.

## The fix

- `tools/verify_route.py` replays `replays/toystory2_player_andys_house_v2.pad` (5 segments, 2400
  recorded pad frames) and judges the RAM dump at pad frames 3600 and 4000. `--negative` replays the
  same recording and judges it at pad frame 1200, inside its first segment and before any press
  reaches the game: the arrival predicate must say no there, so it is shown to be sensitive rather
  than always yes. (A shorter recording was tried as the negative — `replays/
  toystory2_intro_movie_skip_v1.pad` — and rejected: skipping the intro movies leaves the front end
  idle long enough for the guest's own attract demo to enter the level, so Buzz exists and it would
  have made the negative claim something false.)
- Its default binary is `build/bin/toystory2_port`, the path `tools/verify.py` builds. It had
  defaulted to `build/verify/bin/toystory2_port`, which no tool in this tree builds or writes — the
  gate had been judging a stale binary left in an obsolete build layout.
- `tools/ts2_route.py` keeps only what it owns: the exact-frame tap grammar and its compiler
  (`Tap`, `parse_tap`, `compile_pad`) that `tools/headless_run.py --tap` uses. The named route table
  and its CLI are gone; nothing else referenced them.

## Evidence

`--route` on the recording: `[route] pad frame 4000: Buzz x=194774 y=60026 z=-361401`, every cited
instruction of `tools/ts2_guest_words.py` matching, `faults=0`, `[route] PASS` (exit 0).

`--negative` on the same recording at pad frame 1200: `Buzz exists=False; 1 problem(s)`,
`[negative] PASS: the predicate rejects it` (exit 0).

`--determinism`, the recording replayed twice: identical RAM-dump SHA-256s at 3600 and 4000,
identical picture SHA-256s at 2400, 3600 and 4000, identical non-black counts (662730/691200 and
662577/691200), `[determinism] PASS: 2 RAM dumps, 3 pictures compared` (exit 0).