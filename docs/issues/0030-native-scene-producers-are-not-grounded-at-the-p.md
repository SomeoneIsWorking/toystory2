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
`0x800100E4(mesh, scale, materialDepthTableIndex, 0x1F800384)` begins by consuming the signed mesh
header word. Its third argument is not a material-table pointer: the submitter shifts it by two and
adds it to the pointer loaded from `0x800A12F8`.

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
ordinary guest bodies through scoped dynarec dispatch. The capture is active only around a resident update, bounded by
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

The first source decoder is now binary-grounded before any GTE operation or packet write. A positive
signed mesh header `N` places `N` eight-byte vertices at `mesh+4` and its command stream at
`mesh+4+8*N`; a non-positive header uses `-N` vertices and places the stream at `mesh+8+12*(-N)`,
accounting for the extra four-byte header and one four-byte auxiliary record per vertex. Each base
vertex is signed `x/y/z` halfwords followed by the 15-bit colour that the submitter expands from bits
`10..14`, `5..9`, and `0..4`. Command words select opcode `word&31`; opcodes `0..15` consume 12-byte
primitive descriptors, `16..23` consume four-byte descriptors, and `24..31` terminate. Even
primitive opcodes load four packed byte vertex indices and odd opcodes load three. The upper command
bits retain the exact material-table byte offset `(uint16(word)>>4)&0x1F0` (equivalently the
five-bit slot `(word>>8)&31` scaled by 16), blend variant `(word>>5)&3`, and signed material-state
update condition. The source starts at `0x800CD2E0`, and every non-negative command replaces that
state with the raw record at `0x800CD200 + offset`; this is independent of the entry argument's
depth-table index. The packet construction paths further prove the 12-byte group's
packed texture-coordinate selection: quads consume the four attribute halfwords in order, while
triangles skip the low halfword of the first attribute word and consume the remaining three. The
decoder retains both those selected words and the raw payload; CLUT/texture-page adjustments and the
resolved texture/material resource still remain unclaimed.

`resident_mesh_format.*` implements those checked source layouts and a command walker bounded by the
remaining canonical 2 MiB RAM range and the retail terminator. `ResidentSceneHistory` retains the
first decoded primitive and per-mesh opcode/material/blend/primitive-count summary without consuming
guest GTE, OT, or GP0 output. The census runs inside the `0x800100E4` observation wrapper before its
scoped call to the ordinary guest body, so it samples source RAM without replacing guest rendering. Its focused
boundary covers a non-negative state update followed by negative inheritance, plus a separate
negative-first stream that retains the initial `0xE0` record. This is static decoder evidence pending an authorized real-disc histogram;
it is not a visible native producer.

No native renderer is retained or wired while those semantics are unknown. The next authorized
real-disc run must first report the resident command/material denominator and raw records; only then
may an independently grounded 4:3 world producer be implemented and picture-compared. Native and
temporal capabilities stay disabled until that picture exists.
