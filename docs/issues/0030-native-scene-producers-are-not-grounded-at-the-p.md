---
id: 30
title: Native scene producers are not grounded at the pre-GTE title boundary
status: investigating
symptom: Guest widescreen crosses the 512-pixel VRAM parity and Native plus lerp have no authored producer state
tags: native-renderer,widescreen,interpolation,render,re05,re07
state_items: S009,S010,S011
created: 2026-08-27
updated: 2026-08-27
---

The controlled `aspect=1` real-disc run completes 300 native-owned frames but presents vertical black
slabs because expanding the resident guest draw canvas from 512 to 684 crosses each fixed horizontal
VRAM parity. Crop, stretch, OT replay, and post-GTE packet translation are invalid fixes.

Live frame-59 instrumentation measures the real pre-GTE frontier: `0x800100E4` submits 2,381 GTE
operations, `0x80017FF8` submits 801, `0x8001C920` submits 562, and `0x8001DFE4` submits 383. Static
xrefs place these beneath scene/object owners `0x8002622C`, `0x80026D34`, `0x80024174`, and
`0x8002518C`, all orchestrated by resident scene root `0x8002A070`. The root is called by both resident
update owners `0x8007B254` and `0x8007B850`.

The first authored state boundary is now identified statically. Both update owners call
`0x8002C848(0x800C1540)` before the later `0x8002A070` scene root; several other update calls intervene,
so these are producer and scene boundaries rather than adjacent calls. The argument is the live camera structure:
signed 32-bit position at offsets `+0/+4/+8`, followed by three 12-bit angle values sourced from
halfwords at `+12/+14/+16`. `0x8002C848` copies the position to scratchpad `0x1F800374`, derives its
coarse position at `0x1F800384`, converts the angles into the rotation matrix at `0x1F800394`, and
publishes the angle triplet at `0x800A1618`. Scene root feeds that matrix, position, and its scene-list
pointer to culling owners `0x8001ECD4`/`0x80020074`, which build the two visible-object pointer lists at
`0x800BB4D8` and `0x800C0AB0` before the mesh submitters run.

Dual-method xrefs over all 148,992 resident words independently confirm both lists and their count /
residency state. The cullers return the primary and secondary counts in the low/high halfwords; scene
root passes those exact stack-local counts with the two list bases to `0x8002622C`. The persistent
globals at `0x800A146C` and `0x800A14F4` are not those draw counts: `0x80027724` merges newly visible
records against the prior resident set, loads and releases resources, filters the result, and writes
the filtered count for the next frame. Reading those globals after resident update would therefore
mislabel a residency set as the submitted list.

The common `0x8002622C` path establishes the smallest typed instance seam. Each list element points at
a 0x14-byte visibility record: position at `+0/+4/+8`, radius at `+0x0C`, type/flags at `+0x0E`,
viewport at `+0x0F`, and object record at `+0x10`. The object record selects the active resource at
`+0x18 + 4*mem32(0x800A11E0)` and the instance at `+0x14`. Common instance families carry position at
`+0/+4/+8`; their mesh is at `+0x14` or `+0x1C` according to the type family. The dominant leaf
`0x800100E4(mesh, scale, materialTable, 0x1F800384)` begins by consuming the signed mesh header word.

The first implementation slice retains that camera input in the title's per-Core context after
each resident update. `ResidentCameraHistory` stores stable previous/current signed positions, masks
the authored rotations to 12 bits, and interpolates rotations across the shortest wrap path. Bounded
real-disc re37 (PID 3059921) exits at 120/120 reconciled frames with zero dropped layers and no guest
VSync or fatal. Its opt-in `ts2-camera` trace proves the live capture changes from
`(194774,34831,-399753)/(39,0,0)` to `(197184,44313,-350379)/(37,487,0)` while two inspected captures
remain coherent Andy's Room gameplay.

`ResidentSceneHistory` now observes those two exact runtime entries without replacing guest behavior.
Its `0x8002622C` wrapper captures the live list/count, packet pool and view selector plus typed
visibility/object/resource/instance/mesh candidates; its `0x800100E4` wrapper captures the actual mesh
pointer, signed header word, scale, material table and coarse-camera pointer. Both then call their
compiled generated bodies directly. The capture is active only around a resident update, bounded by
the retail merge owner's 256-entry batch contract, and retains previous/current frames in the
per-Core title context. The Clang product builds, the focused boundary passes 13/13 with 110 checks,
and the full C++ format/tidy policy passes.

Bounded real-disc re39 (PID 3138207) is the first live witness for this observation seam. It exits
itself at 120/120 reconciled frames with zero dropped layers and no guest VSync, fatal, or watchdog
timeout. Across 89 resident scene updates the exact owner always submits four batches while the total
candidate count changes from 142 through 198 and the dominant mesh-call count changes from 66 through
86. The first visible record changes from `0x801467B8` to `0x801467A4`, with corresponding object,
instance, and candidate-mesh changes; the first actual submitted mesh is `0x8015A4E0` with signed
header word 16. Inspected 960x720 captures at frames 60 and 120 are distinct coherent Andy's Room
gameplay frames with intact room geometry, characters, effects, and HUD. This run is diagnostic
producer evidence rather than final product certification because the shared psxport tree changed
during its active Clang build. A subsequent clean Clang build against landed psxport `3c342ec3`
passes all 20 CTest gates. Its exact-product re40 rerun (PID 3160059) exits at 120/120 reconciled
frames with zero dropped layers and no guest VSync, fatal, or watchdog timeout. It reproduces the
same 89 live scene updates and 142..198 candidate / 66..86 mesh-call ranges. Inspected frames 60 and
120 remain coherent and distinct; a pixel-change instrument first validated on the identical-frame
control reports 671,652 of 691,200 pixels changed (97.1719%) between them. This is now clean live
reachability evidence for the input seam, but still not evidence of a native-rendered layer.

The remaining implementation must decode `0x800100E4`'s vertex and primitive groups plus their
material/texture inputs, submit equivalent native geometry and 2D layers, and enable Native and
temporal capabilities only after real-disc picture comparison. The typed entry arguments and their
live reachability are grounded; the mesh's internal vertex/primitive stream and texture/material
semantics are not.
