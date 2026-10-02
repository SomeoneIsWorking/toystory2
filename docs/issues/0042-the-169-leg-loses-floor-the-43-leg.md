# 0042 — the 16:9 leg loses floor the 4:3 leg draws, from inside the original view

**Status**: open, reproduced, cause not yet identified.

## The defect

`replays/toystory2_player_andys_house_v2.pad`, pad frame 2500, 16:9 at 1280x720
(`scratch/wide/user/after3_2500.png`) against the same frame at 4:3
(`scratch/wide/user/c43_2500.png`). Both captures were opened and inspected; the earlier
"percentage of lit pixels in the margin" metric is what mis-called this, and it is wrong —
a correct widening legitimately ends in empty room.

What is wrong is measurable without interpretation:

- The wide leg is **not** a horizontal shift of the narrow one. The wooden post sits at sink
  839..895 in both, and every other feature tracks: 4:3 `274..468` / 16:9 `275..468`,
  `687..698` / `687..698`, `509..524` / `509..524`, `691..706` / `691..705`.
- Under that alignment (guest column *g* at 4:3 sink `160 + 1.875g`, at 16:9 sink
  `1.871(g + 86)`) the two frames are **bit-identical** over guest columns 0..466 at y=700 and
  over most of the frame.
- From guest column ~472 the 4:3 leg keeps drawing floor to its own edge (column 511) and the wide
  leg does not. At y=700: 4:3 is `(132,74,25)` at sink 1080; 16:9 is `(33,33,33)`. The floor is
  missing from **inside the original 4:3 view**, not only from the revealed margin.
- What is left is not unwritten VRAM and not a stretched backdrop: the `(33,33,33)` region is a
  solid uniform fill (87% of a 170x90 sample), and `(33,33,33)` occurs 4900 times in the wide
  leg's colour census against 13 times in the 4:3 leg's — it is the guest's own flat backdrop
  polygon, drawn across the widened canvas, with the floor that should cover it not submitted.
- The truncation is not a rectangular clip. The rightmost content column per row runs 683 at
  y=0..390, 621/606/614 at y=420/450/480, 605 at y=540, 683 at y=630, 558 at y=660, 553 at
  y=690 — a geometry-shaped, per-row boundary.

## What was ruled out

Three genuine 4:3 horizontal bounds in `game/render/resident_widescreen.cpp`'s owners were
widened in turn. Each was confirmed present in guest memory at pad frame 2300 by dumping RAM,
and each left the frame-2500 capture **byte-identical**, so none is the gate and none was kept:

1. `FUN_8001FB64` +1044 (`0x8001FF78/84/90/9C`) — the four-vertex reject,
   `beq` past it when all four projected x are `>= 512`. The renderer root `FUN_8002A070` calls
   this quad drawer at `0x8002A1D0`. (`0x8001FFA8`, `ori $t1,$t1,0x200` in the delay slot, is a
   flag bit and not a width.)
2. `FUN_80027AF0` +1484 and following — eight `slti $v0,$v0,0x200` at
   `0x800280BC/D4`, `0x80028344/5C`, `0x800285CC/E4`, `0x80028854/6C`, the per-edge reject inside
   the object-visibility leaf this owner already overrides as `kObjectCullLeaf`. Widening the
   rectangle the leaf is *handed* cannot reach it, because the bound is an immediate.
3. The same leaf's screen-box clamp, `addiu $s6,$zero,0x200` at `0x80027B48`, the width stored as
   each object box's right edge (`sh $s6,($s0)`).

All three were reverted: a patch to guest code with a measured effect of zero is not a fix and
would only hide the real gate. **The gate is still unidentified.**

## Notes for whoever picks it up

- The disc stores instruction words **little-endian** and the decomp pipeline reports addresses
  against a text base of **`0x8000F800`**, so disc offset + `0x8000F800` = runtime address. Getting
  this wrong puts you 0x7800 low and every "function" you disassemble is a different, plausible-
  looking function; the running address of `FUN_8002A070`'s body is confirmed by dumping RAM.
- The floor path reached from `FUN_8002A070` is not fully mapped: the renderer's own `jal` targets
  in `0x8002A070..0x8002A800` do not include `FUN_8001FB64`, so the call the pipeline attributes
  to `0x8002A1D0` and the bytes at that address disagree. Resolve that before trusting any
  "the renderer calls X" claim.
- `FUN_80020074` and `FUN_8001ECD4` are unexplored world-draw routines and are the next place to
  look for a bound that is not the literal `0x200` — one loaded from the display environment or
  the published screen rectangle.
- Captures: `scratch/wide/user/{c43,before,after,after2,after3}_2500.png`, the aligned-strip and
  difference images beside them, and `scratch/wide/png.py` (scratch-only PNG crop/compare, since
  the locked environment has no Pillow or numpy).