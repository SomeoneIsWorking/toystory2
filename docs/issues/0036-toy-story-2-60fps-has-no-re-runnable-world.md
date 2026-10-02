---
id: 36
title: Toy Story 2 60fps has no re-runnable world producer to interpolate
status: resolved
symptom: the framework gate for a guest-owned in-between on the shipping Gte path is now built and measured, and the in-between field itself was built and produced an exact submission — but the guest fills ONE ordering table per frame, so the re-run's write moves that frame-level ownership and the NEXT field's own submission runs away (58,243 nodes where it entered 896) until the render queue refuses at its cap
state_items: S011
tags: fps60,interpolation,render-queue,gte-path,temporal
created: 2026-10-02
updated: 2026-10-03
---

**Resolution (2026-10-02):** chose capture-based per-vertex interpolation by GTE projection provenance (psxport `ProjectionProvenance` + `GuestGeometryInterpolation`, title scopes in `resident_projection_scopes`) over re-running the world pass or native producers, because it moves camera AND character animation, never re-runs or writes the guest, presents real frames verbatim, and applies to every TS2 GTE producer that the title can scope; the re-run record below is history.

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

## How the world pass actually decomposes (measured)

The proposed split — a guest-state half (camera load, cull `0x8002622C`, merge arrays, OT cursor) and
a draw-only emitter — does **not** map onto the guest's structure. The call chain is:

| routine | insns | role |
|---|---|---|
| `FUN_8002A070` | 679 | scene root: camera load, per-view compose, merge arrays, cull, OT cursor |
| `FUN_80027724` | 243 | merge-array allocator |
| `FUN_8002622C` | 706 | **object-type switch that both tests and submits** |
| `FUN_800100E4` | 4806 | mesh submitter: GTE composition, vertex transform, packet build |

`FUN_8002622C` is one `do { } while` over candidates whose `switch` on the object type both
frustum-tests the object and then calls `FUN_800100e4(...)`. Culling and submission are interleaved
inside a single pass, so there is no "culled set" object for a separate emitter to consume. The camera
is applied *inside* `FUN_800100E4`, which never references the published view block
`0x1F800394` / `0x1F800384` / `0x1F8003B4` at all.

A native transcription would therefore be roughly 6400 instructions — about five times the camera
producer's 1273, which RE-05a measured as not reaching parity.

**But the camera seam is explicit and narrow, which makes a cheaper route possible.** The camera enters
per object as a *matrix*, loaded into GTE control registers CR0..CR4 before the submitter call:
`FUN_80083c84` (`SetRotMatrix` from `0x1F800394`) and `FUN_8001f4b0` (55 insns, composes and writes the
same CR0..CR4). `FUN_8002622C` selects which one per object:

```c
puVar7 = &DAT_1f800394;                                    /* slot 0 = the main view */
if ((*(byte *)(piVar8 + 0x19) & 7) != 0)
  puVar7 = (undefined1 *)(((*(byte *)(piVar8 + 0x19) & 7) * 0x20) + 0x800A8340);  /* 8 slots */
FUN_8001f4b0((int)piVar8 + 0x12, puVar7);
```

Eight per-object view matrices live at `0x800A8340 + n*0x20`. So the camera is a *parameter of the
submit*, not a thing fused into the geometry — which is exactly what interpolation needs, and it is
what the title's captured `ResidentMeshSubmission::cameraCoarseAddress` records.

## The re-run experiment: measured, and it FAILS

Bar as set: neutral means all guest-visible state — RAM, scratchpad, GTE, CPU registers, device
writes — identical after a re-run except redirected emissions, measured by the framework's own
`MachineSnapshot` / journal, with `PSXPORT_OVERRIDE_DIFF_DEAD_STACK=0` so nothing is excused, and
**nothing restored**.

**A single re-run is nearly free.** In a captured gameplay frame, running `FUN_8002622C` a second time
and diffing the snapshots either side of it:

| | result |
|---|---|
| RAM | 4 ranges, 12 bytes, all at `0x1FFEF0..0x1FFF08` — the re-run's own callee frame |
| scratchpad | 0 bytes |
| GTE | 0 registers |
| COP0 | 0 registers |
| `hi` / `lo` | unchanged |
| GPR | 3: `v0` 0x00000000 → 0xFFFFFED9, `t0` → 0x500, `t1` → 0x801DD4E0 |

`v0` is the pass's own return value — `FUN_8002622C` opens with `local_6c = 0xfffffed9` and returns it
— and `t0`/`t1` are caller-saved, outside the register set the contract judges. The framework's own
differential, run on the same address with the dead-stack window closed, independently reported
`registers_differing: 1` (that `v0`), `memory_ranges_differing: 4`, `memory_bytes_differing: 12`,
`dead_stack_bytes_ignored: 0`. The rendered frame stayed at the guest reference, 502,034/691,200.

**The first repeatability attempt was itself wrong, and the correction matters.** The fault below came
from re-running through `callOriginalToReturn`, which dispatches with the CURRENT register file.
`FUN_8002622C` takes its arguments in `a0..a3`, and the real call has already returned with those
caller-saved registers clobbered — so the re-run was handed a stale merge-array pointer instead of the
live one, and read through it at the routine's first array access. A re-run has to be invoked with the
arguments it was given.

This is NOT restoring state to fake neutrality. It is making the second dispatch the same call the
first one was. Whatever the second call WRITES is still measured, still listed, and still left
standing.

```
[executor:error] resident-world-neutrality-repeat required a completed guest call, but execution
exited as fault at 0x8002630C after 186 cycles: Lightrec execution fault
```

`0x8002630C` is +224 bytes inside `FUN_8002622C`, whose body opens
`iVar10 = *(int *)(local_50 * 4 + local_68);` over the merge array in `local_68` (= `param_1`). A
fault that early, on the entry region's first array load, is what a bad argument looks like — not
evidence about the pass.

### Repeatability, with the arguments the pass was actually given

Instrumented through the existing `resident-scene-observer` owner (so there is one owner of
`0x8002622C`, not two), with `a0..a3` captured at override entry and restored before the second
dispatch so the re-run is the same call the first one was, and `sp` verified identical before and
after the real call (`0x801FFF58` both times, so the real call leaves the stack balanced).

**`0x8002622C` has three call sites inside the scene root**, told apart by the return address. The
residue is not a property of the routine, it is a property of WHICH caller:

| return address | `a0` (the array) | residue |
|---|---|---|
| `0x8002A468` | `0x801DE18C` — a caller-stack local | 4 ranges / 12 B |
| `0x8002A494` | `0x801DE650` — a caller-stack local | 5 ranges / 10 B |
| `0x8002A4EC` | `0x800BB4D8` = `&DAT_800bb4d8` — the persistent merge array | **0 ranges / 0 B / 0 GPR** |

The `&DAT_800bb4d8` call is the one `0x8002a070` makes with the scene's own persistent merge array, and
it re-runs **exactly neutral**: not one byte, not one register, GTE included. The other two pass an
array that lives in the caller's stack frame, and they leave 10-12 bytes in the callee's own dead
frame below `sp` plus the three caller-saved registers.

### The fault, captured — and then it stopped reproducing

Restoring the argument registers did not change the fault: `0x8002630C` after 186 cycles still
occurred within the first handful of invocations, on different invocations run to run. So the
stale-argument explanation is disproved, and the fault outlived it.

The routine's own code says a "consumed resource" could not be the cause: it READS the merge array
(`iVar10 = *(int *)(local_50 * 4 + local_68)`) and never writes it, and it only ever READS the objects
that array points at — no store through `iVar10` anywhere in its 706 instructions. Two readings of a
non-mutating walk cannot diverge.

The probe was then changed to dispatch through `psx::cpu::callOriginal` (which RETURNS an
`ExecutionResult`) instead of the title's `callOriginalToReturn` (which ABORTS on a non-return), so
that a fault could be captured rather than killing the run. With that change:

| configuration | re-runs | faults | worst residue |
|---|---|---|---|
| persistent-array call site only (`ra=0x8002A4EC`) | **1300+** | **0** | **0 ranges / 0 B / 0 GPR** |
| all three call sites | **4100** | **0** | 5 ranges / 12 B / 3 GPR |

**The fault does not reproduce under either configuration.** The 12-byte worst case is the callee's own
dead frame below `sp` (`0x1FFEF0`..`0x1FFF08`), the same bytes the first measurement found, and the
three GPRs are `v0` — the routine's documented return value — plus caller-saved `t0`/`t1`.

**Confirming the dispatch is not the cause.** The probe was put back on the title's ABORTING
`callOriginalToReturn` — the same path that used to fault — and rerun over the whole route: **4100
re-runs, 0 aborts, 0 faults.** So the dispatch is exonerated; that is not where the fault came from.

**So the variable was the call-site gate all along.** The runs that faulted re-ran EVERY captured call
to `0x8002622C`; the runs that do not fault re-run only the three scene-root call sites
(`0x8002A468`, `0x8002A494`, `0x8002A4EC`). The three that were sampled are all clean. The conclusion
is therefore that `0x8002622C` has at least one caller BEYOND those three that faults when re-run,
and that caller was never identified, because the runs that faulted stopped before the site could be
logged. It is recorded as a named unknown, not as a defect in the pass: the pass re-runs state-neutrally
across every call site measured.

The per-call-site difference stands regardless of the fault: the persistent-array call is neutral
outright, and the two caller-stack-array calls are neutral up to their callee's own dead frame.

**Not a verdict, and not the opposite of one either.** The earlier reading — that roughly three
re-runs exhaust something and the cheap route is closed — is withdrawn: the fault is not a consumed
resource, it lands in the entry region's first array load, and at least one call site re-runs exactly
neutral. What is missing is the root cause of the fault, and until it is found the ~6,400-instruction
native producer remains the only route with a demonstrated method behind it.

### Where the in-between field must be emitted from — measured, not designed

The first wiring emitted the field from the guest's own scene pass, in the `0x8002622C` override
immediately after the real call. Instrumented, the gate works and the blocker is different:

```
scene owner: 200 calls, 19 matched ra=0x8002A4EC
scene owner: 400 calls, 69 matched ra=0x8002A4EC
in-between field SUPPRESSED: fps60 has no redirect open during the guest's own scene pass
```

**`rqRedirect` is not open while the guest builds its scene.** It is open only during fps60's own
re-render, which happens *after* the guest's pass has already run. So an in-between field emitted from
inside the guest's pass has nowhere to submit into, and will always be suppressed — the pass would run
clean, draw nothing, and be indistinguishable from a correct implementation.

The emission therefore has to happen where the redirect IS open: from the title's
`TemporalSceneSource::reconstruct(Core&, float t)`, which the framework calls inside the re-render
with the isolated queue and the display scope already in place. That method does the same three steps
the override was going to do — lerp the camera structures, re-run the guest's cull/submit, restore —
but at the only moment the destination exists.

This is the framework's intended seam for exactly this, and reaching for it earlier would have avoided
a pass that looked alive and rendered nothing.

### Two real defects found on the way to the presenter, and the wall behind them

1. **The temporal presenter was never committed.** `FramePresenter::commit(Core*, int)` forwards
   `temporal = nullptr`, and that overload takes the real-frame path every time — the presenter is
   constructed, never invoked, its previous-endpoint flag never rises, and `eligible()` is never even
   asked. The title now passes `game->temporalPresentation.get()` explicitly.
2. **`enhancementsAllowed()` is `mPath == RenderPath::Native`**, and `Fps60::active()` requires it. A
   `Gte` default therefore disables fps60 permanently, whatever the capability object says.

With both fixed the in-between field emitted for the first time — the lerp, the neutral re-run and the
redirect all work. And then the wall:

> **Enabling interpolation requires the Native render path, and the Native path does not reproduce the
> guest picture: 502,034/691,200 on Gte, 421,935/691,200 on Native.**

The capability stays `widescreenOnly()`. The landed 16:9 work outranks enabling 60fps, and a title that
declares a capability whose default path changes the picture would be lying about what it ships. The
real remaining work is therefore to make the Native path reproduce the guest's frame — which is a
larger piece of rendering work than this milestone, and not something to smuggle in behind a
capability flag.

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
## The gate was the wrong shape — fixed, and kept

The premise above was that the Native path had to reproduce the guest's picture before anything could be
presented. It does not. The Gte path ALREADY rasterizes the guest's own GP0 primitives into the
composite, from the guest's own ordering table, through the same queue; the only thing missing was
permission, and the re-runnable producer this issue said did not exist. Four framework changes, all
still landed:

1. **`TemporalSceneSource::reconstructsGuestGeometry()`** (default false). A source that re-runs the
   guest's own submit is not a PC enhancement of the guest picture — it is the guest's picture at
   another instant — so it is permitted on the Gte path. `Fps60::interpolationPermitted()` is the one
   answer every gate reads; `RenderMode::enhancementsAllowed()` is untouched and still Native-only for
   every other enhancement.
2. **`TemporalSceneSource::capturedQueueIsComplete()`** (default false). A source whose captured queue
   already IS its resolved submission presents that queue verbatim at the real frame — a per-SLOT
   decision keyed on `t`, not a property of the source. Getting it wrong in either direction is silent:
   skipping the real frame's reconstruction hands "the real frames are unchanged" to a producer that
   never ran (measured — this session), and reconstructing it replaces the guest's measured picture with
   a replay of itself.
3. **`DisplayPassGuard` is no longer armed for a guest-geometry reconstruction.** The guard is a
   read-only-overlay invariant; the guest is not a read-only overlay, and arming it aborted the run on
   the re-run's first store (`[mem] [pc_render VIOLATION] guest write to 0x80000000`).
4. **`gpu_ot_replay_field(Core*, maxNodes)` + `gpu_ot_field_node_count(Core*)`** (`core.h`,
   `gpu_native.cpp`, `ordering_table.h`): re-walk the guest's last ordering table through the same
   walk, classification and rasterizer the real frame is queued by, bounded to the node count the
   field's own walk entered. The bound is the refusal, not a truncation: an in-between of the same scene
   cannot be a longer draw list than the field, so a longer one means the reconstruction's write left the
   chain malformed.

`RenderCapabilities::guestInterpolated()` is the profile that would declare this (Gte default, temporal
interpolation, no Native path). The title does NOT declare it — see below.

## The in-between field was built, and it is the submission that fails

The re-run was built end to end and produced a correct picture. What it does not do is leave the guest
alone. Measured, in order:

- the world pass is not one call. `0x8002622C` is called FOUR times per field by the scene root
  `0x8002A070`, in two pairs differing only by `a3` (the viewport selector), and the world is the
  83-candidate call at `a0=0x800C0AB0`, not the 2-candidate call an earlier attempt re-ran:

  | ra | a0 | a1 | a3 | what it is |
  |---|---|---|---|---|
  | `0x8002A468` | `0x800BB4D8` | 2..6 | 0 | small batch, other viewport |
  | `0x8002A494` | `0x800C0AB0` | 83..84 | 0 | **the world**, other viewport |
  | `0x8002A4EC` | `0x800BB4D8` | 2..6 | 1 | small batch, viewport 0 |
  | `0x8002A518` | `0x800C0AB0` | 83..84 | 1 | **the world**, viewport 0 |

- a call is its arguments, its stack, its merge array and the per-object skip byte at
  `object + sceneBank + 0x26` (the real call leaves every drawn candidate holding a nonzero byte, so a
  re-run without the entry values skips the whole world), and the camera is three regions, not one.
- with all of that captured and restored, the reconstruction's ordering-table walk entered the SAME node
  count as the field's own walk (2,004 at fence 588) — the submission was right.
- and then the NEXT field's own submission entered 58,243 nodes where it had entered 896, the render
  queue filled to its 65,536-item cap and the run aborted: `[rq:error] FATAL: render queue full
  (65536 items) — refusing to drop prims (fail-fast) … 65469 repeating geometry already submitted this
  frame … VERDICT: RUNAWAY`.

Three attempts at isolating the re-run's write, and what each measured:

| attempt | result |
|---|---|
| rewrite the field's own nodes (restore the pool pointer, the OT cursor and the list head/tail to the field's first-call values) | 2,004 nodes — exact. Still the runaway below, because the re-run also moves state the guest did not hand it. |
| let the re-run append, and replay the chain past the field's own node count | the guest fills ONE table per frame and its chain does not close over an appended suffix: 63,229 nodes, same abort |
| let the re-run append into its own `DrawOTag` table | the guest issues one `DrawOTag` per FRAME, not per pass, so the re-run never has a table of its own; replaying the last one re-walked the field's own chain and appended to it |

So the wall is not the geometry and not the projection — it is that a present-time re-run cannot hand
the guest's frame-level draw-list ownership back. A producer that corrupts the next frame is worse than
no producer, so the title stays on `widescreenOnly()`, and `ResidentWorldPass` keeps only what is
correct and useful: the two adjacent presented fields' camera, captured at the present boundary (the
capture-at-update-top bug is fixed and recorded), with the source declining eligibility and the reason
written where the next attempt will read it.

## What Andy's House cannot show either way

Measured per field, the guest's camera moves ≤26 units per field on packed 1.3.12 values near 6600 —
under a thousandth of the view — and 204 of the first fields after a level load have a camera delta of
exactly zero. A camera interpolator therefore cannot produce a visible in-between in this room at all:
the visible motion between two presented fields is character animation, computed by the guest from its
own clock inside the mesh submitter. Criterion 3 below is not reachable here even with a correct
producer, and that is a fact about the room, not about the producer.

## What remains

1. Identify the frame-level state the re-run moves and does not restore — the guest's ordering-table
   ownership is the prime suspect, and the OT walk's own per-frame bookkeeping (`s_prim_order`,
   painter replay order, `s_cur_node`) is the second, since the reconstruction's walk mutates it and the
   next field's walk reads it. Restore both, then re-measure the next field's node count; it must be the
   896 the field's own walk entered.
2. Then the producer question in criterion 3 stands on its own: with the camera endpoints correct, a
   scene whose camera moves more than a projected pixel per field would show a true in-between. Andy's
   House does not, so the evidence has to come from a turn, a cutscene or a second room.
3. If no such scene exists for this title, the honest product answer is that a camera interpolator
   cannot make this game 60fps, and the animation clock is the producer that would have to be carried
   across the two endpoints.
