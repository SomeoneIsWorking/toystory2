---
id: 27
title: Toy Story 2 FMV overlay owns a blocking guest movie loop
status: closed
symptom: FMV/FMV.BIN function 0x800D7088 decodes and presents every STR frame inside one guest call and directly calls linked VSync 0x80088628 once per movie frame
state_items: S003,S007
tags: frame-loop,vsync,fmv
created: 2026-08-27
closed: 2026-10-03
updated: 2026-10-03
---

## Root cause

Retail `0x800D7088` performs the whole STR open/demux/MDEC/upload/display loop and calls the linked
`VSync(0)` at return PC `0x800D7590` once per movie frame. Its only direct caller is `0x800D6628`.
One guest call therefore spans an unbounded presentation loop, which no one-turn finite budget can
hold; the native frame owner must take the loop, one display field per step, and never let the guest
VSync succeed.

## What is owed

`psx::fmv::play` is a native blocking movie owner, but it returns a frame count and does not preserve
the guest contract's playback-mode skip result, so it cannot simply be substituted. The FMV loop owner
must present the skip outcome retail returns, with one display field per host frame.
## Resolution (2026-10-03) — already resolved by the Game::fmv override; verified, not re-archived

Checked before closing, because the issue asked whether that was so. It was: `game/fmv/guest_movie_player.cpp`
replaces `FUN_800D7088` (the FMV overlay's movie player) with a native owner, and it satisfies every
thing "What is owed" listed. Nothing was changed to close this.

**The blocking loop is gone, one movie frame per host turn.** The retail body is not entered at all:
the override reads the guest's own path argument, asks `Game::fmv` for one movie frame, and returns the
turn with `requestExecutionExit(CooperativeYield, 0x800D7088)`. Resuming re-enters the override, so the
guest call stays suspended inside the retail player exactly as it would inside the guest's own
streaming loop — but the host gets a turn between movie frames, which is what makes the movie
interruptible at all.

**Measured, an unmodified idle boot** (no pad, no skips), four movies in sequence:

```
guest call front-end movie returned after 0 display field(s) in 440 turn(s)   toy2fmv/acti.str
guest call front-end movie returned after 0 display field(s) in 158 turn(s)   toy2fmv/dlogo.str
guest call front-end movie returned after 0 display field(s) in 303 turn(s)   toy2fmv/tt.str
guest call front-end movie returned after 0 display field(s) in   ... turn(s)  toy2fmv/traler2.str
```

**ZERO display fields** is the load-bearing number: the guest's per-movie-frame `VSync(0)` never
succeeded even once, and each movie frame was one host turn. That is "one display field per host step,
never let the guest VSync succeed", measured rather than asserted — a run with a single guest VSync
completing would report a non-zero field count here.

**The skip outcome is the retail one.** The override returns `_DAT_800A1670` when the guest skipped and
zero at end of movie, which `FUN_800D6628` hands back to the front-end sequencer unchanged; nonzero is
what makes Start skip the REMAINING movies as well as the current one. Measured both ways:

| run | movies opened | outcome |
|---|---|---|
| idle boot, no input | 4 (`acti`, `dlogo`, `tt`, `traler2`) | the whole intro plays out |
| `replays/toystory2_intro_movie_skip_v1.pad` (Start at frame 413) | 2 (`acti`, `dlogo`) | the cold intro ends; the remaining movies never open |

Two movies rather than one is the correct signature, not a partial skip: the flag makes the sequencer
treat the intro as finished, and the movie the Start lands in still has to play out its own frame and
return. That is the behaviour the contract comment predicts, and it is the difference between
"Start skipped this movie" and "Start skipped the intro".

**No guest VSync trap fired** anywhere in either run (0 hits for the `kVSyncTrap` window at
0x80088628..0x80088770). The run also ends cleanly with the fallback ledger printed:

```
run-end: guest: calls=4811 translated_blocks=1898 executed_blocks=1453562 executed_instructions=13211137
run-end: fallback: calls=0 instructions=0 refused_calls=0 compilation_failed=0 self_modifying_code=0
                   unsupported_block=0 load_delay_hazard=0 unsafe_instruction_fetch=0
```

S007's movie-presentation item was closed separately in issue 0040, where the movie became visible to a
headless capture; that is a separate defect and did not affect this one.
