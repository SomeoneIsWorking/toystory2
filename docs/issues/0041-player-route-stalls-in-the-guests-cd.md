---
id: 41
title: The player route stalls in the guest's own CD-ready wait after the level start
status: closed
symptom: With the front end reached by pad input as a PLAYER (`DAT_800A120C == 0`), the level start publishes Andy's House (`DAT_800A16A8 == 1`, `LEVEL01/LEVEL.BIN` authenticated) and then never returns; the guest sits in its own interrupt-driven CD-ready wait and no further picture is presented
tags: cd,level-start,player-route,frame-loop
created: 2026-10-02
updated: 2026-10-03
---

## The symptom was real; the CD diagnosis was wrong

There is no CD wait. The `DAT_8009FDF0 == 5` reading that opened this issue is boot-time residue, and the
level start was never inside `FUN_8008B0D0`.

**`DAT_8009FDF0` is written once, at boot.** Its only writer is `FUN_8008B6DC`, reached only from the
one-time init `FUN_8003A780` -> `FUN_800812DC` -> `FUN_8008B338` -> `FUN_8008B0D0(1)`. Nothing on the
level-start path calls it, so `5` is simply the last thing boot left there. Its neighbours
(`DAT_8009FDF8 = 0x8007F7A8`, `DAT_8009FDFC = 0x80088FA4`) are that same boot-established callback pair,
for the same reason. Reading them as a live wait is what made the CD path look guilty.

## What the guest is actually parked in

The resume PC is `0x8007C3E0`, which `--function-at` places **+156 bytes inside `FUN_8007C344`**
(`0x8007C344..0x8007C5F7`), the level start's own transition fade. That routine is the wait:

```c
if ((param_2 == 0) || (param_2 == 0x7b)) { bVar1 = false; iVar5 = 0; }   // param_2 = DAT_800A120C
...
do {
  FUN_8003fa68(1);                                     // one display field
  if ((bVar1) && (iVar2 = iVar2 - DAT_800a1174, ...)) iVar2 = 0;   // countdown ONLY if bVar1
  if ((((DAT_800a1480 & 0x4000) != 0) && ((DAT_800a11e4 & 0x4000) == 0)) && (!bVar1)) {
    FUN_80077598(0,0,0,0xc); bVar1 = true; ...          // rising Cross -> arm the countdown
  }
  ...
} while (iVar2 != 0);
```

`FUN_8007BEC4` calls it as `FUN_8007C344(DAT_800a16a8, DAT_800a120c)`. On a **player** level
`DAT_800A120C == 0`, so `bVar1` starts `false` and `iVar2` is **never decremented**: the loop's only exit
is the rising Cross edge (`0x4000`, i.e. the guest's X/confirm word) that sets `bVar1`, after which the
28-field countdown runs and the loop ends. That is the guest's own "press X to skip the level
transition", and it is why the attract route never stalls: `DAT_800A120C != 0` there, so `bVar1` starts
`true` and the countdown is armed immediately.

Measured at pad frame 1800 on the stalled route, exactly that state: `DAT_800A1480 == 0x8` and
`DAT_800A11E4 == 0x8` (Start held, **no Cross edge anywhere**), `DAT_800A120C == 0`, `DAT_800A1174 == 1`.
The second early-out is dead for the same reason:

```
if ((((DAT_800a1480 & 1) != 0) && (!bVar1)) && (-1 < _DAT_800c166c)) { ...; uVar6 = 1; bVar1 = true; }
```

`_DAT_800C166C` is `-30`, and it is `-30` **forever**: an exhaustive scan of every store in the
executable whose immediate is `0x166c` finds three writers, all in `FUN_8007A9E8`'s boot init
(`-1`, then `-30` and `-30` again), and none of them increments it. `-1 < -30` is false, so the
countdown arm can never fire and the Cross edge is the sole way out.

## Resolution: nothing to fix in the port

The input path is intact — the port delivered every press it was given, and the recording simply never
pressed X during the transition. Adding a Cross press to the level phase of the route is the whole fix;
no override, no CD change, no delay, and no drive timing were added.

`replays/toystory2_player_andys_house_v2.pad` (phase-keyed, 5 segments, 2400 frames, card sha256
`77d33c6be1b8862c8b55d6159ba9a6aed172778a64b6ae9aa705f0638c0330cb`) is that recording:

- `guest call resident level start returned after 177 display field(s) in 244 turn(s)` — the level start
  **returns**, which was the acceptance condition;
- `ResidentPreparation::step` reports `ready` and the outer loop enters the resident phase
  (`TEMP phase=7` for the rest of the run) — **Andy's House reaches the play loop**;
- gameplay presents Buzz in Andy's House at 96% non-black, walking under d-pad input, with the guest's
  own HUD (`scratch/headless/work/scratch/screenshots/present_1900.png`, `present_2360.png`). Room 1's
  toys are Buzz, Trixie and a Zurg figure; **Woody is not in room 1**, so "Woody visible" cannot be met
  from this route — he is the game's own subject elsewhere, and the only Woody capture in this workspace
  is the boot title screen (`present_2900.png` of the attract run). Reported rather than faked.
- The attract route is unaffected: `replays/toystory2_andys_house_area_transition_v1.pad` still returns
  its level start in 28 display fields (event 0), as before.

## What this supersedes

`DAT_8009FDF0 == 5` must not be read as a live CD wait, and `ts2::cd::FileTransfer` is exonerated: the
instant-CD design is not implicated in anything observed here.