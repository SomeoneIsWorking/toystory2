# 0042 — the 16:9 leg loses floor the 4:3 leg draws, from inside the original view

**Status**: fixed (display-area origin, `resident_widescreen.cpp presentField`). The residuals below are scene content.

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

## ROOT CAUSE FOUND AND FIXED: the presented window was one margin too far right

The defect was **not** in psxport's draw path. Depth and scissor were both cleared of suspicion on
inspection and by measurement:

- `render_pass_set` clears the depth attachment on every band (`dt.load_op = SDL_GPU_LOADOP_CLEAR`,
  `clear_depth = 0.0f`), and the world test is `kWorldDepthCompare = GREATER_OR_EQUAL` (reversed-Z),
  so the backdrop cannot leave depth the floor fails against and the bands cannot poison each other.
- The 3D scissor is `sc3d = {sx*scale, sy*scale, disp_w*scale, h*scale}` — the display rect, which is
  correct — and the 3D band is in fact **empty in both legs** (`--debug ires` prints
  `tri=0 tex=0 semi=0` at 4:3 as well as 16:9: the guest uses an OT0 list, so nothing is depth-tagged).
- The presenter builds `p.disp[0] = in.sx * scale` against `p.disp[2] = content_w * scale`, where
  `content_w` falls through to `disp_w` (the port's 684) because `game_guest_picture_is_native_width`
  is false here.

The cause is the pairing of that origin with that width. The title publishes the draw area and draw
offset at the margin (`canvasOriginX_` = 86) — which is right, because `ws_2d_local_x` offsets
everything the guest submits by (wide - native)/2 and that is what lands the guest's 512-wide frame at
canvas 86..597 — but it published the **display** area at the same origin while the presenter's
`present_display_width` returns the **whole 684-wide canvas**. Origin and width therefore described
`canvas[86, 770)`: the left margin's 86 columns were never presented at all, and the rightmost 86
columns of the window read past the canvas into unwritten VRAM. The floor was being cut one margin's
worth, which is exactly the width the "missing" strip had.

FIX (`game/render/resident_widescreen.cpp`, `presentField`): the display area starts at the canvas's
own origin, `displayAreaStart(0, canvas_.top)`. The draw area and draw offset stay at the margin.

MEASURED at pad frame 2500, the floor's right edge per sink row:

```
   y     origin 86   origin 0
 660        1045        1206
 690        1036        1197
 715        1030        1191
```

exactly one margin (161 sink columns) further right, and now past the 4:3 frame's own edge at sink
1117; the previously cropped left margin now shows the quilt wall. 4:3 is **byte-identical** before and
after (`cmp` clean), which it must be — this only runs when the widening is active. Captures:
`scratch/wide/user/sx0_2500.png` (after) against `after3_2500.png` (before), `sx0_43_2500.png` (4:3).

This also explains every earlier null result at last: the floor was never culled, never frustum-tested
and never unsent — it was submitted, rasterised, and then not presented.

## The two residuals after the fix

**(b) the left margin's dark slanted region — NOT A DEFECT, it is real geometry.** The pixels there
are quilt-wall blue, not void: over the sampled region the dominant colours are `(16,33,74)` (1863 of
3225 samples), `(0,25,49)` (742) and `(0,33,66)` (630), and `(16,33,74)` is exactly the blue the guest
draws on the quilt at guest column 0 of the 4:3 frame. The "dark slant" is a shadowed facet of that
wall continuing into the margin. Nothing to fix.

**(a) the bottom-right wedge and the right-edge band — the guest's own backdrop where the room ends,
not a presentation fault**, on this evidence:

- The void colour is the guest's `bg=1` backdrop, `rgb(32,32,32)`. That is the same fill the 4:3 leg
  shows ABOVE the room's ceiling — the dark band across the top of every 4:3 capture is this same
  backdrop, so void where the room has no geometry is how this title renders, in both legs.
- The void's shape is stable while the scene animates: backdrop samples in the two regions are
  643/720 and 141/336 at frames 2500, 2650, 2800 AND 2950 — identical proportions 450 frames apart.
- The guest submits geometry well past the canvas in exactly those rows (vertices to x=739 at canvas
  rows 210..239, and to the GTE's overflow x=1023 elsewhere), so nothing is being clipped at the
  canvas edge as a class; the presented content simply stops where the guest stopped drawing.

LIMITATION, stated rather than hidden: **the camera does not pan on this route.** Verified directly:
at 4:3 frame 2950 the framing is unchanged from 2500 with Buzz merely larger and turned, so there is
no frame of `toystory2_player_andys_house_v2.pad` that brings these areas inside the 4:3 view, and the
direct 4:3 reference the comparison wanted could not be obtained on this route. And the primitive CSV
records only 2 of up to 4 vertices, so it cannot prove coverage of the void regions either. The
conclusion is therefore "consistent with the room's own boundary", NOT proven.

## HUD anchoring at 16:9 — MEASURED

`replays/toystory2_andys_house_pause_v1.pad` (new; the v2 route with a 6-frame Start press added in
the resident phase, same card sha256 `77d33c6b...`) opens the pause menu, giving the first HUD-bearing
16:9 gameplay frame. Captured at both aspects at pad frame 2900
(`scratch/wide/hud/present_2900.png`, `present_2900_43.png`).

The menu panel's 1px outline, found as the longest near-black horizontal run:

```
        x extent        width   centre x
 4:3    460..820          361     640.0
16:9    460..820          361     640.0
```

and the glyph runs inside the panel outline (measured strictly inside x 464..816 so a wall star cannot
contaminate them):

```
               4:3                16:9              ratio
 PAUSE MENU   x 526..753 w 228   x 526..753 w 228   1.0000 / 1.0000
 EXIT LEVEL   x 477..800 w 324   x 477..800 w 324   1.0000 / 1.0000
```

So the centred HUD stays centred — centre 640.0, the sink centre, in both legs — keeps its authored
width exactly, and its glyphs are pixel-identical in position and size. A full horizontal stretch would
have measured 1.333; the measured ratio is 1.0000. That is the widescreen rule satisfied for a centred
element: centred stays centred, nothing stretches.

## Edge-anchored in-level HUD — NOT FOUND in reachable content, so no anchoring change was made

The widescreen rule also requires edge HUD elements to move to the widened edges. **That precondition
could not be evaluated, because no edge-anchored in-level HUD element exists in any reachable content.**
No native anchoring adjustment was written, because an unmeasured change is exactly what must not ship.

What was checked, and what it showed:

- **Routes driven with pad replay only (no RAM writes).** `toystory2_player_andys_house_v2.pad` reaches
  Andy's House as a player; idle-at-title attract reaches the arcade interior and the Wild West barn
  yard. Captured at 4:3 (`scratch/headless/work/scratch/screenshots/present_3300.png`, `present_6100.png`,
  both inspected at full 1280x720): **no in-level HUD in either**. The barn yard shows only the
  "DEMO MODE" attract caption, which is attract presentation, not the gameplay HUD.
- **Primitive evidence, not just the picture.** `PSXPORT_PRIMDUMP` over resident gameplay at 16:9 frame
  2495 (`scratch/wide/user/prims_16x9.csv`) yields exactly 14 sprites: one full-canvas `bg=1` backdrop
  (`op 0x60`, x 0..684), one `op 0x60` fill (x 0..512), and twelve `op 0x65` textured cells forming the
  room's own 64x32 tile grid at x -21..427. None is a HUD element, and none sits at a widened-edge
  position. The remaining 6267 textured quads in that frame are the world, not UI.
- **RE.** Ghidra identified the resident per-frame loop as `FUN_8007B254` (from `residentUpdateAddress`,
  `game/loop/outer_loop.h:85`) — the same function that reads the demo flag `DAT_800A120C` six times —
  and its draw pass `FUN_80078780` / `FUN_80074F80` / `FUN_8007863C` / `FUN_80046A88` / `FUN_8002A070`.
  Eight functions from that pass were decompiled. All are animation/scroll state updates:
  `FUN_800775CC` has `jal=0` and calls nothing, so it draws nothing; `FUN_80078780` maintains frame
  counters. None computes a screen x for UI, and the executable contains no `lui/ori` materialisation
  of a GP0 `0x65` command, so the drawer could not be located by command-writer either.

**Scope of the claim.** This is "not present in the content this port can reach", NOT "Toy Story 2 has
no in-level HUD". Levels with collectibles or damage states were never reached, so a HUD that draws
only once a collectible counter is non-zero cannot be excluded. Reaching one is the next step, and it
needs a pad route that selects a level other than Andy's House — not a code change.

## Edge-anchored UI DOES exist on the stage-select screen — and it is correct by design

A front-end sweep of `toystory2_player_andys_house_v2.pad` locates the stage-select screen at pad frame
~1000 (title at 500, main menu 600-950, stage select 1000-1040, then the level load). It is the only
edge-anchored UI found anywhere in reachable content, and it carries three elements:

| element | anchor | sink x, 4:3 | sink x, 16:9 |
|---|---|---|---|
| `ANDY'S HOUSE` | top centre | centred | centred |
| `00` (level number) | bottom LEFT | 358..398 | 358..398 |
| `○ SELECT` / `○ BACK` | bottom RIGHT | 858..1068 | 858..1068 |
| red panel | full frame | 190..1119 | 190..1119 |

Captures `scratch/wide/sel/{4x3,16x9}/present_1035.png`. Every element measures **identical** at both
aspects, so the bottom-right pair sits at its 4:3 x inside the 16:9 frame and the panel is pillarboxed
rather than widened.

**That is the documented intent, not a defect.** `ResidentWidescreenProjection::syncToGuestDisplay`
takes a `residentFrame` flag and the class comment records why: "The front end publishes 512-wide
screens of its own (the Level map) ... every front-end screen is presented at the width it authored,
centred by the letterbox." An earlier attempt that widened this very screen kept the panel at its
authored width while the widened display area made the guest lay the map preview out beside it, and the
menu stopped filling its own frame. So the stage-select screen is evidence for the scoping decision,
not for a change.

## What the widening does to an edge-anchored 2D element, if one is ever found

Worth recording for whoever reaches a level with a real HUD, because it decides whether a change is
needed at all. The presenter maps guest VRAM columns straight onto the widened canvas
(`guestClipLeft = presentationExtent.width - guestDrawWidth`, which is 0 for a full-width 684 canvas),
so a guest element at column 0 already sits on the widened left edge, while one anchored to the guest's
literal 512 would stay at 512 and would NOT reach the widened right edge at 683. The title already
tells the guest its canvas is 684 wide (`widenDrawEnv` writes `drawWidth()` into the draw env's width
field), so a drawer that derives its right edge from the draw env follows the widening on its own; one
that anchors to a literal 512 would need a title-owned adjustment. No such drawer has been located, so
nothing has been changed on that basis.

## Packet evidence (`PSXPORT_PRIMDUMP=2495:2505`)

Captured at both aspects for the same route and frame, `scratch/wide/user/prims_4x3.csv` and
`prims_16x9.csv`. Note each leg has its OWN draw-area origin, so guest coordinates are recovered by
subtracting it — 512 for 4:3 (`dax0=512, offx=512`), 86 for 16:9 (`dax0=0, offx=0`, the port's
margin). Vertical mapping is identical in both legs (707 of 720 rows agree at guest column 0), so
only the horizontal window differs and there is no stretch.

**1. The `(33,33,33)` fill is the guest's own BACKDROP, drawn UNDER everything — not a mask over the
floor.** In the wide leg, OT id 0 is `op 0x60` (a rectangle) with the background flag set, spanning
the whole canvas, `rgb(32,32,32)`:

```
16:9  id 0    op 60  bg=1  guest rect (-86, 0)..(598, 240)  rgb 32,32,32
```

The 4:3 leg has **no such primitive** — its only `op 0x60` is an unrelated small rectangle (16:9 id
13, `rgb 8,32,0`). Because it is OT id 0 it is the first thing drawn, so it can only ever be a
background showing through where nothing follows, never an overdraw. Its own edge is already
correct under the widescreen rule: a screen-filling primitive spanning the widened canvas.

**2. The floor is submitted at PIXEL-IDENTICAL guest coordinates in both legs.** Matching on
(op, x0, y0, x1, y1, r, g, b, textured), **1140 of the 1183 4:3 primitives reappear in the 16:9 leg
with identical geometry and identical shading**, including 87 textured Gouraud/quad floor primitives
that span the disputed band — e.g. 4:3 id 179 `x 471..595 y 168..268 rgb 144,120,112` is 16:9 id
277, the same numbers; 4:3 175/176 are 16:9 271/272; 4:3 292 is 16:9 396; 4:3 1182..1185 are 16:9
1328/1329/1340/1341.

So the wide leg draws the floor at exactly the coordinates the narrow leg does. That eliminates the
whole family this issue has been chasing — no cull, no frustum plane, no FOV, and no missing
submission can explain a hole in geometry that is present, identical and submitted. The earlier five
no-effect patches are consistent with that, and the H=256 probe is consistent with it.

**3. What the wide leg actually adds:** 204 primitives that exist only at 16:9 (margin content), 322
prims reaching past guest x 512 against 50 in 4:3, and the one full-canvas backdrop of (1).

**Therefore the open question is now sharply stated:** identical floor primitives at identical
coordinates are rendered by the 4:3 leg and covered by the backdrop in the 16:9 leg. The only
differences between the legs on the draw path are the backdrop's presence, the draw-area origin
(512 vs 0), and the presented window. The next thing to test is **depth and ordering against that
backdrop** — whether it writes depth at a plane the floor loses — before looking anywhere else. A
depth/dither flag on the primitive would settle it; the CSV does not carry the depth bit, which is
the one piece of attribution still missing.

Also settled along the way: the guest's projected screen x legitimately reaches the GTE's signed
16-bit overflow (prims at x 1023 are geometry far outside the view, clipped by the draw area), and
`op 0x60`/`0x3C`/`0x34`/`0x38` are rectangle / textured Gouraud triangle / textured Gouraud quad /
flat-shaded triangle respectively.

## What was ruled out

Five candidates, each widened in its owner and each verified present in guest RAM by dumping at pad
frame 2300. None changes the capture, and none was kept.

**The frustum side-plane hypothesis is FALSIFIED, with a direct experiment.** The room/floor
drawer `FUN_80020074` programs the GTE's H register (COP2 register 17, at `0x8800`) itself, from an
immediate of its own rather than from the guest's shared projection — H being exactly the
frustum side-plane half-angle:

```
0x80020198  addiu $t0, $zero, 0x1000   ; H = 4096, the 4:3 half-angle; V = 3072 at 0x8002019C
0x800201A0  mtc2  $t0, 0x8800
0x80020538  addiu $t0, $zero, 0x1999   ; the same mode-A pair again later in the body
0x80020540  mtc2  $t0, 0x8800
```

(the mode-B path instead takes H from the sector record, `lhu $t0, 0xC($s1)` + `srl 2` at
`0x800201D4`/`0x800201DC`, so it carries no immediate). Setting those to the correctly widened
`4096 * 684 / 512 = 5472` changed the frame by **zero pixels**. So a probe was run with H forced to
**256** — a 21x change in half-angle. That probe DID change the frame: geometry visibly compressed
toward the centre, the crib bars and wardrobe narrowing. So the routine executes, and its H governs
this scene's projection. But the floor's right edge is **pixel-identical at H = 256, 4096 and 5472**:

```
   y   H=4096   H=5472   H=256
 400     1279     1279    1279
 500     1208     1208    1208
 660     1045     1045    1045
 715     1030     1030    1030
```

**The truncation is invariant to the horizontal projection scale, so it is not a frustum, a
field-of-view, or any screen-space cull.** What is missing is geometry the room drawer never puts
into its visible list at all — `FUN_80020074` builds two lists, walking them into `DAT_800BB4D8`
and `DAT_800C0AB0` and counting into `iVar3`/`iVar4`, and the floor simply is not in them past that
point. That is where the next attempt should look, and it is a submission question, not a
clipping one. (Careful: Ghidra shows no bounds check in that append, so "the list is full" is not
established — only the absence of the geometry.)

The other four, all literal `0x200` horizontal bounds, likewise verified in guest RAM:

1. `FUN_8001FB64` +1044 (`0x8001FF78/84/90/9C`) — the four-vertex reject,
   `beq` past it when all four projected x are `>= 512`. (`0x8001FFA8`, `ori $t1,$t1,0x200` in the delay slot, is a
   flag bit and not a width.)
2. `FUN_80027AF0` +1484 and following — eight `slti $v0,$v0,0x200` at
   `0x800280BC/D4`, `0x80028344/5C`, `0x800285CC/E4`, `0x80028854/6C`, the per-edge reject inside
   the object-visibility leaf this owner already overrides as `kObjectCullLeaf`. Widening the
   rectangle the leaf is *handed* cannot reach it, because the bound is an immediate.
3. The same leaf's screen-box clamp, `addiu $s6,$zero,0x200` at `0x80027B48`, the width stored as
   each object box's right edge (`sh $s6,($s0)`).
4. The published submission rectangle is NOT the gate either: `widenScreenRect` already publishes
   `(-4096, 4096)`, far outside the observed boundary.

All were reverted: a patch to guest code with a measured effect of zero is not a fix and
would only hide the real gate. **The gate is still unidentified.**

## A correction that will bite the next attempt

The immediate of an `addiu <rt>, <rs>, imm` is the **low** halfword of the instruction word, and this
disc is little-endian, so it is written at the instruction's own address. Writing it at `site + 2`
rewrites the opcode instead: the first attempt at this fix destroyed the picture completely (the
run diverged to an unrelated scene) before the offset was corrected.

## Notes for whoever picks it up

- The disc stores instruction words **little-endian** and the decomp pipeline reports addresses
  against a text base of **`0x8000F800`**, so disc offset + `0x8000F800` = runtime address. Getting
  this wrong puts you 0x7800 low and every "function" you disassemble is a different, plausible-
  looking function; the running address of `FUN_8002A070`'s body is confirmed by dumping RAM.
- The floor path is `FUN_80020074`: it takes a camera matrix and position, walks the room's sector
  records at `DAT_800A8860` (26-byte records, but the polygon chain is LINKED through the record's
  `+0x10` field and each polygon advances 0x14, not a fixed stride), and appends surviving polygons
  into two visible lists at `DAT_800BB4D8` and `DAT_800C0AB0`. Its own per-polygon tests are
  `SX3 + radius > 0`, `SX3 + radius - |SX1| > -1`, `SX2 + radius - |SX3| > -1` and
  `SX1 - radius - 4*DAT_800A134C < 0` — all measured against screen column **0**, never 512, so the
  right-hand side of this room is not gated by them. That asymmetry (everything tested against the
  left edge) is itself worth a look.
- 66 sites in the executable program GTE register 17 (H). All but the two above read the guest's
  shared projection, which the port already widens.
- The renderer's own `jal` targets in `FUN_8002A070..0x8002A800` do not include `FUN_8001FB64`, so
  the call the pipeline attributes to `0x8002A1D0` and the bytes at that address disagree. Resolve
  that before trusting any "the renderer calls X" claim.
- `FUN_8001ECD4`, the second world-draw routine, has no `mtc2` to register 17 anywhere in its body
  and is unexplored.
- Captures: `scratch/wide/user/{c43,before,after,after2,after3,h169b,hprobe}_2500.png`, the
  aligned-strip and difference images beside them, and `scratch/wide/png.py` (scratch-only PNG
  crop/compare, since the locked environment has no Pillow or numpy).