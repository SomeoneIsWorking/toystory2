---
id: 35
title: Toy Story 2 loading removal has a design but no landed owner
status: resolved
symptom: Every file load still went through the guest's blocking whole-file read, the two unbounded loader loops were still guest code, and no loading-only wait or skip route was removed anywhere in the product
state_items: S015
tags: loading,cd,iso9660,overlay,fmv,pad-cancel
created: 2026-10-01
updated: 2026-10-02
---

## What the product does today

The guest's own loader owned every CD file read, including the whole-file read primitive at
`0x80082608`, which issued `CdRead` and spun on `CdReadySync(0)`. There are no boot logos to skip.
The resident level start was also replayed by hand: `ResidentPreparation` wrote ~51 guest words and
re-ran the transition loop itself.

## What was true when this was opened

## What has landed

1. **`ts2::cd::FileTransfer`** — a native override of `0x80082608` that reads from the disc image the
   title already has authenticated (the same owner `game/overlay/` uses) and returns the exact
   `CdlFILE` size the guest expects. This removed the blocking whole-file wait: the same 1000-field
   boot route measures 23.5 s against the guest loader's 30.2 s, and the picture at field 900 is
   byte-identical (502,426 non-black pixels). It refuses rather than transferring on a path the disc
   does not have, a destination outside guest RAM, a file that will not fit, or a sector that cannot be
   read, and `tests/toystory2_cd_hle_boundary.cpp` asserts the refusals leave the destination untouched.
2. **Bounded loops, at the two owners that contain them** — `0x80082728`'s two unbounded retry loops
   are now a bounded, refusing retry policy (`0x80082728`, 3 attempts, the guest's failure value when
   none succeeds), and the `0x80082648` search spin is gone with the read itself, so all three spin
   sites named here are native. `0x8007F174` is still the guest's, in the file *processor* rather than
   the loader.
3. **The recovered skips are reachable and proven** — the front-end movies return at 194, 41, 73 and
   69 display fields on the standard route (a full movie is thousands), each one the guest's own
   Start/Cross cancel ending the call, and the Level-1 "PRESS X" card's Cross at `0x8007C448` takes
   the route into the level.
4. **The level start is the guest's own route** — `ResidentPreparation` used to write ~51 guest words
   (`[0x800A1174]`, `[0x800A1480]`, `[0x800A155C]`, `[0x800A1370]` and the rest) and re-run the
   transition loop. Retail `0x8007BEC4(level)` at `0x8007AE14` writes every one of them; it now runs
   as one resumable guest call spanning 100 display fields, each presented by the host, with its one
   real wait (`FUN_8003FA68`) already natively owned. The picture at field 900 is unchanged
   (502,426 non-black pixels).
5. **The play-loop entry state is the guest's too** — the twelve words the frame driver still
   published are `0x8007A9E8`'s own block at `0x8007AE20`, reached after a successful level start and
   stopping at the loop's own two-field barrier `0x8007AEAC`. `ResidentPreparation` now runs that block
   as a second resumable call, so the transition flags, the exit countdown, `[0x800A1430] =
   [0x800C166C] << 1` and the fade are written by the guest stores that own them (including the
   halfword stores the hand-written word writes did not reproduce) and the replay is gone. The guest
   is left parked in its own barrier and the host owns the loop body from the next field, as it
   already did for every later field.
6. **The last unbounded wait is bounded** — `0x8007F174`, decompiled whole, is not a wait for anything
   this product owes the guest: `0x8007F170` stores zero into `$s0` and `0x8007F180` is `beqz $s0`, so
   `do { VSync(0); break 1; } while (true)` is the sound-bank routine's own assertion that the bank it
   searched fits the window it accepts, unbounded in retail too. Its containing routine `0x8007F108`
   is registered so its own body runs through the framework's bounded resume loop: nothing is skipped,
   a processor that returns is untouched, and one that reaches the assertion costs bounded display
   fields and ends as a named refusal instead of a spin.

## Verification

The same 1000-field route, before and after: every capture across every load — the four intro movies at
100/200/260/340, the title and its Start cancel at 420, the level-select map at 640, the "PRESS X" card
and its Cross at 800, the level start at 860/900/950 and gameplay at 999 — is byte-identical before and
after, the four movies still return at 194/41/73/69 display fields and the level start still spans 100,
so the route's authored field count is unchanged. Wall clock is NOT a usable comparison from this
session: the shared machine's load average went from about 2 when the "before" run was taken (24.01 s)
to 14+ later, and the SAME after-binary then measured 29.52 s and 29.07 s against its own 22.49 s and
22.46 s earlier. Re-measure both legs on an idle machine before quoting a time.

## Next step

Nothing in the load path is a host-owned wait any more. The remaining S015 question is what the level
transition looks like from the player's side once a second level is reachable (issue 34).