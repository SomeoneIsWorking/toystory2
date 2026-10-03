# `tools/verify_movement.py --run` never reaches Andy's Room at HEAD

**Status**: open (diagnosed 2026-10-03 during the structure pass; not fixed there)

## What happens

`uv run --frozen python tools/verify_movement.py --run` fails every time at `b82293c`:

```
[movement] idle 900->960: PlayerState(x=0, y=0, z=0, yaw=0) -> PlayerState(x=0, y=0, z=0, yaw=0)
[movement] Up held 40 pad frames: horizontal travel 0 position units (Z 0 -> 0)
[movement] FAIL Buzz does not exist at the start of the idle window
```

Every sampled word is zero, and the guest pad word is `0x0000` at every sample.

## Why

The run is not a product defect; the ROUTE is stale. `verify_movement` compiles
`ROUTES["andys-room"]` plus the movement taps with `ts2_route.compile_pad` into an **absolute,
unkeyed** schedule of `RUN_FRAMES = 1100` frames counted from boot (`scratch/headless/run.log`):

```
[padrec] replaying 1100 frame(s) in 1 UNKEYED (absolute from boot) segment(s)
[fmv] toy2fmv\acti.str   -> ... 440 turns
[fmv] toy2fmv\dlogo.str  -> skipped by Start at frame 59,  61 turns
[fmv] toy2fmv\tt.str     -> 303 turns
[fmv] toy2fmv\traler2.str -> began after pad frame ~900 and was still playing when the run ended
```

Each intro movie is a native host slice (`ts2::fmv` yields one movie frame per host turn), so the
four movies consume most of the 1100-frame budget before the front-end poll is ever reached. The
gameplay taps are therefore delivered while the front end has not yet polled, and Buzz never exists
at the sampled frames. Under machine load it is worse: the movies take longer, and the budget is even
further consumed.

This is exactly the failure RE-23 describes for unkeyed recordings, and the fix already exists in
this repo: `InputPhase` (`game/input/recording_phase.h`) plus a **phase-keyed** recording. The
working route to the same gameplay is `replays/toystory2_player_andys_house_v2.pad`, which
`tools/headless_run.py --pad` replays through the level start and into Andy's House.

## Acceptance

`tools/verify_movement.py --run` passes again when the judge runs a phase-keyed route to Andy's Room
(same sampled frames and Buzz words as today, `tools/ts2_guest_words.py`) instead of an absolute
1100-frame schedule. The `--negative` control (same route, no gameplay input, must FAIL) must fail
for the same reason it does today.
