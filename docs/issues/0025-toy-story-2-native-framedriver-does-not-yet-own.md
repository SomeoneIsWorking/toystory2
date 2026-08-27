---
id: 25
title: Toy Story 2 native FrameDriver does not yet own independent MEMORY/FMV loops
status: investigating
symptom: The native frame driver owns main's finite pre-resident, resident, and post-resident routes, but independent MEMORY/FMV loop owners and a real product run across the new VSync trap remain
tags: frame-loop,vsync,host-ownership,resident,overlay,re18
created: 2026-08-27
updated: 2026-08-27
---

## Classified cause

Toy Story 2 has more than one loop owner. Resident main `0x8007A9E8` contains a two-field barrier and
normal/alternate update legs, while `BITS/MEMORY.BIN` and `FMV/FMV.BIN` contain independent VSync
callers. The retired host-turn field clock hid that ownership by servicing guest VBlank asynchronously.

## Current ownership

The title runtime supplies a FrameDriver and splits the measured `0x8007A9E8` boot prefix from its
non-returning main. One finite state-machine step owns cold front-end setup, a single front-end event
poll or interactive selection iteration, resident preparation, and normal `0x8007B254` or alternate
`0x8007B850` update. Transition/front-end frames own one display field; resident frames own the
measured two fields. Every frame owns direct deferred service, input, audio, and one presentation
commit. MEMORY/display initialization begins inside that first finite
frame. Graphics init `0x8003A218` is routed to a title-local state-only override which preserves its
measured buffer effects while omitting both guest VSync calls; the generated super remains unchanged.
Linked libetc VSync `0x80088628` is fatal inside the exact second HLE window. The field mirror is the
title's measured libetc counter `0x8009FD54`, not another engine's `Timing::frameTick` address.

Three real product launches then classified successive boot callers instead of weakening that trap.
The first reached stock `CdSync` `0x80091898`, whose VSync(-1) calls are only the linked library's IRQ
timeout clock; the measured stock-libcd command/sync/read/search entries now route to psxport's
synchronous CD owner. The next launch crossed CD and reached peripheral init `0x8003EEF0`; Ghidra
identified its waits as linked libpad negotiation, not memory-card work. `game/input/native_pad_owner.*`
now writes the measured retail digital packet at `0x800CF8A0`, owns boot/shutdown, and replaces decoder
`0x8003AC58` so it never polls libpad's VBlank-driven state. The third launch crossed both owners and
reached libgpu timeout arm `0x80088380` from ClearImage. Its paired check is `0x800883B4` and its measured
globals are `0x8009EC20/24`; both now route to the framework's existing synchronous-GPU owner. This
owner is Clang-built and was crossed by the later product runs below before resident preparation and
gameplay.

The next bounded retail launch crossed the GPU timeout owner and wrote presents 1 and 2, but both
player-facing captures were black (zero non-black pixels). It then retried the absent
`/LEVEL00/LEVEL.DAT;1` until the watchdog sampled an ISO directory read inside CHD LZMA
decompression. This was not a CHD/decompressor defect: issue #28 proves the finite driver had dropped
the selected level live in `a0` at retail call site `0x8007AE08`, so `0x8007BEC4` always received level
zero. Bounded follow-up re20 crossed the former LEVEL00 retry, proving the ABI correction, then aborted
at the next intentional VSync fatal: 0x80039D9C called by the blocking 0x8007C344 pre-resident fade.
Its only new presents, 1 and 2, were both 960x720 fully black images.

Issue #29's source slice now replaces the synchronous 0x8007BEC4/0x8007C344 route with a finite
resident-preparation owner. It consumes one authored transition field per host frame, preserves the
retail halfword/byte store widths, and invokes a state-only 512x240 graphics initializer which omits
0x80039D9C's two guest VSync calls. The generated supers remain unchanged. This correction is
Clang-built. Bounded real-disc re21 crosses the correction and exits itself at the explicit 300-frame
cap. Its ledger reconciles 300/300 frames with zero dropped layers, no guest VSync/fatal/miss/unmapped
access/watchdog occurs, and presents 45 through 300 visibly show coherent Andy's Room demo gameplay.
After rebuilding the current product with Clang against shared framework `3a8256e9`, bounded real-disc
re35 (PID 2991368) independently exits at 120/120 reconciled frames with zero dropped layers and no
guest VSync or fatal. Its inspected presents 45, 60, 90, and 120 visibly show coherent Andy's Room
demo gameplay.

The subsequent source slice owns main's resident-exit predicate, common cleanup, reasons 1–5,
demo/sequence bookkeeping, and transitions to cold front end, warm front end, resident setup, or
shutdown. Graphics shutdown `0x8003A838` is also native-owned: it retains synchronous DrawSync,
callback removal, pad shutdown, and ResetGraph while omitting its two guest VSync calls. These source
paths are Clang-built; product execution has not reached them yet.

The first controlled `aspect=1` run uses the same finite owner and also completes 300 frames, but the
picture is not acceptable widescreen. Expanding the resident guest draw canvas from 512 to 684 pixels
crosses each fixed horizontal buffer parity inside 1024-pixel VRAM and presents wrapped/atlas columns
as black vertical slabs. This is a separate render-producer issue (#30), not a frame-loop failure.

## Remaining work

Exercise post-resident transitions on the real product, then measure and migrate the independent live
MEMORY and FMV owners tracked separately by issues #26 and #27.
Source/static inspection is not product evidence: each phase must execute without guest VSync or
`host_turn` while preserving title/gameplay presentation.
