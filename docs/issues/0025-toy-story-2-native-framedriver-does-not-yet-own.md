---
id: 25
title: Toy Story 2 native FrameDriver does not yet exercise post-resident transitions
status: investigating
symptom: The native frame driver owns main's finite pre-resident, resident and post-resident routes, but post-resident transitions are still unexercised on the real product
tags: frame-loop,vsync,host-ownership,resident,overlay
created: 2026-08-27
updated: 2026-10-03

`replays/toystory2_exit_level_v2.pad` (7 phase-keyed segments, nothing dropped) runs pause → `EXIT LEVEL`
→ `ARE YOU SURE?` → results → token screen → level card → back in Andy's House, playable. The transition
takes the outer loop's `0x800C166C`-gated event 1 → `residentSetup` and re-authenticates LEVEL01.

## Root cause

Toy Story 2 has more than one loop owner. Resident main `0x8007A9E8` contains a two-field barrier and
normal/alternate update legs, while `BITS/MEMORY.BIN` (`0x800DEF6C`) and `FMV/FMV.BIN`
(`0x800D7088`) each contain an independent VSync caller. The retired host-turn field clock hid that
ownership by servicing guest VBlank asynchronously; each of those loops needs its own finite native
state owner.

## Current ownership

`game/frame/` splits the measured `0x8007A9E8` boot prefix from its non-returning main, and one finite
step owns cold front-end setup, a single front-end poll or selection iteration, resident preparation,
and the normal `0x8007B254` or alternate `0x8007B850` update; resident preparation consumes one authored
transition field per host frame. Linked libetc `VSync 0x80088628` is fatal
inside the exact second HLE window, and the field mirror is the title's measured libetc counter
`0x8009FD54`. Graphics init `0x8003A218`, the resident fade, and graphics shutdown `0x8003A838` are
routed to state-only native overrides that keep their measured buffer effects and omit their guest
VSync calls.

## Remaining work

Exercise post-resident transitions on the real product. Static inspection is not product evidence: each
phase must execute on the real disc without a guest VSync completing while preserving title/gameplay
presentation.