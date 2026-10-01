---
id: 36
title: Toy Story 2 60fps has no re-runnable world producer to interpolate
status: open
symptom: game_fps60_read_scene_cam is bound and live, but there is no producer the framework's tier-1 can re-run, so PSXPORT_FPS60=1 is refused by declaration and Andy's House never shows an in-between field
state_items: S011
tags: fps60,interpolation,render-queue,gte-path,temporal
created: 2026-10-02
updated: 2026-10-02
---

## What the product does today

`RenderCapabilities::widescreenOnly()` is the honest profile, so the framework refuses the request
before a frame runs:

```
[fps60:warn] interpolated 60fps REFUSED — this title declares no temporal interpolation product (request source: env)
[render:warn] render path 'native' is UNSUPPORTED by this title — using its declared 'gte' path.
[render] render path = gte — geometry from the GUEST (its own GTE + ordering table), rasterized by the
        PC rasterizer (SDL_GPU), PC enhancements LOCKED OUT
```

`scratch/pshot_off/` and `scratch/pshot_on/` are three consecutive presented `shot` captures each, taken
through the debug server on the same route (`--tap 500:start --tap 560:cross --tap 620:cross
--tap 790:cross`, 16:9). Both legs show the live, moving room. They are not byte-comparable: the two
runs were caught at different presented frames (1505 and 1513), which is why the refusal at startup is
the evidence and the pixels are not.

## Why the camera seam cannot carry it

The camera half is done. `game_fps60_read_scene_cam` returns the guest's own published view —
rotation from `0x1F800394`, position from the GTE sub-word block `0x1F800384` — and a full Andy's
House run renders at the guest reference, 502,034/691,200 non-black. What tier-1 needs is a producer
to re-run under the lerped camera, and three independent facts say there is none.

**1. A captured submission cannot be re-projected.** `RqItem` (`render_queue.h`) carries screen verts
`xs/ys`, sub-pixel `xsf/ysf` and per-vertex `depth[]` — and no view or model matrix anywhere. The
camera is fully baked by the time an item is enqueued, so changing it means re-running the GTE, not
editing the item.

**2. The guest's world pass cannot be re-run at present time.** `FUN_8002a070` is one interleaved
routine (679 instructions, no arguments). In order it loads the camera into the GTE
(`FUN_80083c84(&DAT_1f800394)`), composes the per-view matrix (`FUN_8001e8d4(&DAT_800a1618,
&DAT_1f8003b4)`), allocates two 256-entry merge arrays (`FUN_80027724`), runs the culler
(`FUN_8002622c`), projects, and then appends every packet while advancing the guest's own ordering
table write cursor:

```c
FUN_80017c60(uVar4, DAT_800a13f0, DAT_800a142c, &DAT_800a151c);
puVar2 = DAT_800a151c + 1;
*DAT_800a151c = uVar4;
DAT_800a151c = puVar2;
```

Re-running it appends a second copy of the entire world to the guest's OT and draws the merge pool
twice. There is no draw-only re-entry between the projection and the emission.

**3. The framework's tier-1 ownership predicate rejects the guest's world items.**
`LegacyTemporalSceneSource::owns()` claims `item.layer == RQ_WORLD && item.has_xyf`. The guest OT walk
emits with `layer = is3d ? RQ_WORLD : …` (`gpu_native.cpp`) and every `emitOrQueue` zeroes the item
first (`render_queue.cpp:109`, "*it = RqItem{}; // zero every field (… has_xyf …)"), so guest world
items are `RQ_WORLD` with `has_xyf == 0`. A title predicate could claim them by layer alone — but
facts 1 and 2 leave it nothing valid to re-submit.

## Migration action

Own the resident world projection natively: a producer that walks the captured
`ResidentSceneSubmissionBatch` / `ResidentMeshSubmission` inputs the title already records
(`game/render/resident_scene_history.h`, which keeps each submission's eight GTE affine control words,
mesh address and material state) and emits through `RenderQueue::drawWorldQuad`, which is the only
producer that sets `has_xyf` and the only one tier-1 knows how to re-run.

Acceptance criteria, in order:

1. The native producer's packets byte-match the guest's for a captured frame, measured by the
   framework's per-call override differential (`PSXPORT_OVERRIDE_DIFF=<entry>`) — the same bar the
   camera producer failed in RE-05a, which is why the bar is named here rather than assumed.
2. `RenderCapabilities` declares the interpolation product, and a run with `PSXPORT_FPS60=1` logs
   tier-1 activity instead of the refusal above.
3. Consecutive presented `shot` captures in Andy's House show true sub-field motion: the character
   and camera move between presents that are half a field apart, with no blending or pixel sampling.

## Why this is recorded rather than approximated

The in-between field has to be the same source geometry under an interpolated camera. With no
re-runnable producer and no transform on the queue, the available substitutes are all forbidden: shift
a finished frame's camera, blend two presented images, or decorate the final GP0 packets. Each renders
a plausible 60fps that is not the game, and each hides the missing owner.