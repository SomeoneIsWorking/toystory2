---
id: 34
title: Toy Story 2 gameplay input is verified from Buzz's guest-RAM position words, at exact pad frames, with a whole-run ledger on the default report
status: open
symptom: Pad edges were fired by polling the presented-frame counter every 0.2 s of wall clock, gameplay input was only seen in pictures, and a clean run printed no dynarec ledger at all
tags: input,determinism,pad-replay,player-object,ledger,invalidation,perf
state_items: S002,S003,S004,S005,S006
created: 2026-10-01
updated: 2026-10-01
---

## 1. Exact-frame input (cause and fix)

`tools/headless_run.py --tap` polled the control channel's `frame` counter every 0.2 s, so a tap landed
whenever the poll noticed the frame, not on the frame asked for. The framework already owns the exact
facility: `PSXPORT_PAD_REPLAY` (uint16 little-endian active-low mask per pad frame), applied inside
`Pad::serviceFrame` after every other input source, one call per host logic frame
(`CoreResidentFrameBoundary::sampleInput`). `--tap PADFRAME:BUTTON[:HOLD]` now compiles into that file
(`tools/ts2_route.py`); no psxport change was needed. The product runs in its own working directory
(`scratch/headless/work`) because the framework writes `--dump-at` RAM dumps and `--shot-at` pictures
to cwd-relative `scratch/bin` and `scratch/screenshots`, and `scratch/bin` is a provisioned input.

Determinism, `tools/verify_route.py --determinism` (the same route twice, byte-compared): RAM at pad
frames 780 and 900 identical (SHA-256 `de9493f011b4a00f...`, `8a943a72168cd65e...`, 2 MiB each), the
three presented pictures at frames 450, 780, 900 byte-identical (non-black 691173, 690867, 666918 of
691,200 pixels, equal in both runs), tap schedules equal. The same hashes were produced by an earlier
build on psxport 5e34f56a, so they are stable across the framework change. A pad frame is one host logic
frame (one or two display fields, see 0033); the guest sees a press one pad frame after the host mask.

## 2. Route

`tools/ts2_route.py` `andys-room`: start@500, cross@560 (menu), cross@620 (level select), cross@790
("PRESS X"). `tools/verify_route.py --route` passes only if Buzz's object exists in the dump at pad frame
900, nine cited instruction words match that dump, the run exits 0 with the schedule consumed, and the
ledger shows translated code and no fault. `--negative` (route without the last tap) must report Buzz
absent (the object is all zero at frame 780 and until the confirm); it does.

## 3. Buzz's object (RE, from instruction bytes, `external/psxport/tools/disasm.py` on a RAM dump)

`0x8004CF30..0x8004CF94` (resident object updater): `lw v0,0(s0)`, `lw v1,4(s0)`, `lw a1,8($s0)` load X, Y,
Z; `sw` stores them to `+0x5C/+0x60/+0x64` (previous position) and velocity to `+0x68/+0x6C/+0x70`, then
`jal 0x800489C4` (player update). `0x800489C4` reads the control-mapped word `[0x800A15BC] & 0xF0` (D-pad,
`0x80048A04`), then `lw a0,0(s3)` / `lw a0,8(s3)` (X, Z, `0x80048AE8`/`0x80048B00`) and `lh v1,0xE(s3)` (yaw,
`0x80048AC4`). The object is at **`0x800B2188`**. The controller chain: `0x8003B33C` stores the previous
pad word to `[0x800A11E4]` and the decoder's result to `[0x800A1480]` (`0x8003B360`); `0x800734DC..0x80073598`
maps `[0x800A1480]` into `[0x800A15BC]`. All are asserted by `tools/ts2_guest_words.py` against the dump.
Identification by behaviour: the object is all zero until the "PRESS X" confirm, then holds a position
that moves with input and not without it.

## 4. Movement, `tools/verify_movement.py --run` (RAM at exact pad frames)

| pad frame | event | X | Y | Z |
|---|---|---|---|---|
| 900 | idle | 194774 | 60026 | -361401 |
| 960 | idle control (60 frames, no input) | 194774 | 60026 | -361401 |
| 1000 | Up held 960-999 (40 frames) | 194463 | 59401 | -292758 |
| 1060 | idle | 194449 | 59578 | -283030 |
| 1064..1080 | Cross tapped at 1060 (10 frames) | 194449 | 49850, 44218, 42938, 46010, 53178 | -283030 |
| 1090 | landed | 195360 | 59532 | -283029 |

Up: 68,644 position units horizontally in 40 pad frames (idle control: 0). Cross: Y falls 16,640 to its
minimum (PSX Y points down) and returns within 46 units of the start. The no-input run (`--negative`)
reports both effects as 0 and the judge rejects it. Left (an earlier probe) turned Buzz and moved him; the
picture at pad frame 1060 shows him running past the bed. Square changed the yaw word (1527). The pad is
digital only here (`native_pad_owner.cpp` emits a digital packet), so there is no analog input to verify.
The 4096 floor in the judge is arbitrary and far above rounding (observed effects are 16x larger); it is
not a claim about the world-unit scale.

## 5. Whole-run ledger on the default report (psxport 5b66eb7f)

Cause: TS2 does `new Game()` and never deletes it, so `~LightrecExecutor`'s telemetry never ran and a
clean run printed no ledger. The ledger text now has one owner (`runtime/cpu/execution_ledger.*`), used by
the live `guest` command and printed by `native_boot` as `[guest] run-end:` lines;
`tools/headless_run.py` and `tools/verify_route.py` read it (`tools/execution_ledger.py`, a missing
group is a refusal). Route run, 1000 pad frames, headless:

- `guest`: calls=46,107 translated_blocks=8,778 executed_blocks=364,220,145 executed_instructions=1,963,419,994
  host_dispatches=63,933 cache_hits=364,210,495 cache_misses=9,650 memory_callbacks=188,852,521
  invalidations=20,151,652 faults=0
- `invalidations_by_source`: cpu=18,823,584 mapped_store=565,379 dma=760,011 module_load=2,668 native=10
- `budget_exit`: exits=6,251, all 6,251 resume pcs inside a code image
- `fallback`: calls=1,267 instructions=1,267, **all `load_delay_hazard`** (1,267 of 1,267), refused=0, every
  other reason 0.

## 6. What the 20M invalidation notifications cost (measured, not fixed here)

`perf record` (user mode, 499 Hz, 22,170 samples, the 1100-frame route): `lightrec_invalidate_blocks`
is the single largest symbol at **14.5% of user CPU** (its block walk is inlined: 14.2%, of which
`block_overlaps_range` 7.6%). The notification path outside Lightrec (`Core::writeGuestMemory`, memory
callbacks, `lightrec_rw` 2.0%) is small. The new `invalidation_work` line says why: of 15,768,945
invalidation calls that reached Lightrec, 15,420,456 (97.8%) were answered by its translated-word guard
alone, and **348,489 walked every registered block** (847,018,411 blocks examined, ~2,430 per walk) and
**revoked 1,108**. So 99.7% of the walks revoke nothing. Cause, from `blockcache.c` at the pinned
Lightrec 4696481: the guard is a bitmap of words *ever translated* (`translated_words`), not cleared when
a span is revoked, so a store to an already-revoked block's extent still walks the whole cache. The fix
belongs in `shared/lightrec` (clear the bit on revoke, set it again when `lightrec_block_is_outdated`
restores the entry) and needs its SMC differential tests, so it is not done here. No with/without
wall-clock A/B was run: that needs a patched Lightrec and a full rebuild; the profile share is the number.
The 20.15M psxport-counted notifications exceed Lightrec's 15.77M because stores to non-RAM maps never
reach `lightrec_invalidate_blocks`.

Separate finding from the same profile (not investigated): per-block boundary dispatch is ~34% of user
CPU (`resolveHostDispatch` 9.8%, `NativeDispatcher::intercepts` 5.9%, `ImageCatalog::resolve` 5.7%,
`LightrecExecutor::Impl::activeBoundary` 5.6%, `owner` 3.6%, `blockBoundary` 3.5%); each of the
364M executed blocks passes through it.

## Open / unverified

- HUD: no HUD element is visible in the captures at pad frames 900, 1000, 1060 (Buzz in Andy's Room); not
  established whether retail shows one there.
- Pause/unpause (Start in gameplay), camera control, left/right travel, and the other buttons are not in
  the judge; Left and Square were seen once, outside the maintained check.
- `PSXPORT_PRESENT_SHOT_AT` and `PSXPORT_VK_HEADLESS` are reported by the framework as "UNKNOWN knob ...
  matched nothing" although the product reads them through the legacy path (the capture works).
- psxport 5b66eb7f is a local commit on that repository's `ts2-play` branch; this repo's pin names it and
  is therefore not reproducible from a clone until it is pushed.
- Fresh worktrees need `scratch/flat` (`tools/extract_disc_files.py` with `PSXPORT_DISCDUMP` pointing at a
  built `discdump`); without it two selftests refuse, as designed.
