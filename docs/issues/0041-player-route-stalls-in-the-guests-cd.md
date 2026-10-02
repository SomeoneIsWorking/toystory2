---
id: 41
title: The player route stalls in the guest's own CD-ready wait after the level start
status: open
symptom: With the front end reached by pad input as a PLAYER (`DAT_800A120C == 0`), the level start publishes Andy's House (`DAT_800A16A8 == 1`, `LEVEL01/LEVEL.BIN` authenticated) and then never returns; the guest sits in its own interrupt-driven CD-ready wait and no further picture is presented
tags: cd,level-start,player-route,frame-loop
created: 2026-10-02
updated: 2026-10-02
---

## Measurement

`replays/toystory2_player_andys_house_v1.pad` (a phase-keyed recording of Start presses through the
front end, Cross on the main menu, Cross on the level select) reaches the level as a player and stalls
there. Read through `--dump-at` at pad frames 1600/1700/1750:

| word | meaning (from the exact bytes) | value |
|---|---|---|
| `0x800A120C` | the attract/demo flag | `0` — player |
| `0x800A16A8` | the selected level id | `1` — Andy's House |
| `0x8009FDF0` | `FUN_8008B0D0`'s CD-wait mode; case `5` publishes BIOS `0xF2000003` (`CdReadySync`) | `5` |
| `0x8009FDF8` | the BIOS entry pointer that wait calls | `0x8007F7A8` |
| `0x8009FDFC` | the installed continuation it longjmps back through | `0x80088FA4` |
| `0x800A1174` | elapsed display fields | `1` — the loop IS running |

So the guest is inside its own CD-ready wait, taking display fields, and the wait never completes.

## What it is NOT

- **Not the level-start card.** Suppressing `FUN_8007C278` on the player route as well (so no card is
  loaded at all) produces the identical stall in the identical words, with a black sink instead of the
  card on screen.
- **Not attract-only.** The attract route reaches its levels and plays them, so this wait is not a
  defect of the CD owner for every route; something the player route does before it leaves the wait
  unanswered.
- **Not the override's nesting**, now that it is retired on the player route. With the card suppressed
  through the older nested-original override the same state appeared as a hard fault instead of a stall
  (`Lightrec execution fault` at the BIOS trampoline `0x8008B378`, the same wait's continuation), which
  is why the override is now armed per route instead of per call
  (`game/boot/level_start_presentation.h`).

## Where to look next

`ts2::cd::FileTransfer` serves the whole-file read (`0x80082608`) host-side and publishes the guest's
CD mode (`0x80090D40(0xB,0,0)`), but it never issues the guest's own `CdRead`, so nothing ever raises
the CD-ready interrupt this wait is installed against. The player route reaches the wait after the
front end has streamed the story movie through the resident CD path, which is where that bookkeeping
has to be right.

## Acceptance

`DAT_8009FDF0` leaves `5`, the level start returns, `ResidentPreparation::step` reports `ready`, and the
resident play loop presents Andy's House — on the same recording, with no attract flag.
