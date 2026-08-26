# Project state

Factual capability coverage for the Toy Story 2 port. Epic intent lives in
`docs/project-goals.md`, atomic work in `docs/issues/`, ownership in `docs/codemap.md`, and ordered
binary evidence in `docs/re-frontier.md`.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | USA executable, disc files, and loaded code modules are reproducibly identified and placed | verified | — | G001 |
| S002 | Retail resident and overlay code is reproducibly recompiled into the shipping product | partial | S001 | G001 |
| S003 | Guest field service delivers retail callbacks, platform progress, and one presentation commit per field | partial | S002 | G001 |
| S004 | The product boots through the retail front end, title, and LEVEL01 gameplay | partial | S002, S003 | G001 |
| S005 | Host input produces repeatable guest gameplay behavior | partial | S003, S004 | G001 |
| S006 | Guest-rendered 15-bit screens and gameplay present coherently | partial | S003, S004 | G001 |
| S007 | Toy Story 2's 24-bit MDEC movies present coherently | partial | S003, S006 | G001 |
| S008 | Authored projection is published to title-owned consumers | partial | S002 | G002, G003 |
| S009 | Visible scene layers have native game-state producers | missing | S008 | G002, G003 |
| S010 | True widescreen composes correct title and gameplay pictures | missing | S008, S009 | G002 |
| S011 | Presentation interpolates stable authored state to the player-facing 60fps cadence | missing | S008, S009 | G003 |
| S012 | Traveller's Tales `.RAW` assets are reproducibly framed and decompressed | verified | S001 | G001 |
| S013 | The fresh-clone launcher provisions, builds, and starts the intended product | partial | S001, S002 | G001 |

## Current focus

S007 is the current focus: classify the corrupted 24-bit movie field at the source-VRAM, presentation,
or software-reference boundary before changing MDEC placement or display sampling.

## Capability details

### S001 — Reproducible retail inputs and module placement

Evidence: `tools/extract_exe.py` identifies the 598,016-byte `SLUS_008.93` executable with SHA-1
`f90c9cd6…`; `tools/overlay_map.py` derives the LEVEL slot at `0x800D12C0`, the shared MEMORY/FMV slot
at `0x800D5D20`, and the FMV entry at `0x800D6628`. The disc census identifies 22 loaded code modules
without tracking any game bytes.

### S002 — Recompiled shipping substrate

The input-hash-checked emitter produces resident code plus all 22 proven modules, links the product,
and matches the independent CPU oracle 34/34 through the first resident call. The corrected partition
also preserves the model-table reset's branch delay slot.

Gap: seed completeness and indefinite execution coverage remain empirical; future indirect-only roots
and overlay entries must still be derived from execution or binary structure.

### S003 — Guest field service

The title runtime installs the measured graphics-init boundary, invokes registered VBlank
`0x80039D60` at the GPU field rate, advances pad and SPU work, and commits the captured queue through
the shared presentation fence once per field.

Gap: representative long play has not verified timing, audible output, input, and presentation
together, and the 24-bit movie boundary remains unresolved under S007.

### S004 — Boot through gameplay

The generated product renders legal and ESRB screens, enters the FMV module and resident renderer,
reaches a stable title, loads LEVEL01, clears and reloads the measured model slot, and has reached
Andy's Room under bounded host input.

An exact-`99a42aa3` capability run also stayed CPU-active for more than five minutes, advanced disc
activity through LBA 15660, and continued toggling between 24-bit and 15-bit display modes without a
watchdog or fatal diagnostic. The operator stopped its exact PID after the requested frame budget
failed to terminate it; this is sustained-progress evidence, not a successful bounded-run result.

Gap: a representative end-to-end play session is not verified; later overlays, saves, transitions,
and sustained gameplay remain outside current execution coverage.

### S005 — Repeatable player control

The retail pad initializer and independent consumer derive both guest packet destinations. A bounded
host pulse writes active-low Cross and release bytes to the measured slot, and a live run has paused,
unpaused, and moved the camera.

Gap: replaying the exact recorded pad samples diverges at the later pause transition. Issue #20 owns
the deterministic replay investigation.

### S006 — Coherent guest-rendered presentation

The guest-owned field loop commits through the neutral shared fence, and captured legal, ESRB, title,
and gameplay frames demonstrate coherent 15-bit presentation.

Gap: this does not cover the title's 24-bit movie mode, native scene production, wide composition, or
interpolated presentation.

### S007 — Coherent 24-bit MDEC movies

The exact-`dbdb2baf` run switches to 24-bit 256x240 mode and remains live, but present 1500 contains
duplicated noisy columns and is mostly black. Static overlay RE shows the game deliberately selects
24-bit MDEC output, drains block-aligned DMA1 transfers, uploads 16-pixel strips as 24 VRAM halfwords
by 240 rows, and fills a 480-halfword image region. No game-side width correction is supported by that
geometry.

Gap: the retained artifact is a final present only. There is no synchronized raw-VRAM capture or
software-PSX control, and the framework's CPU VRAM-shot conversion shares the present shader's
24-bit byte-stride rule, so those paths are not independent format controls. The guest-visible MDEC
pump's existing differential covers synthetic 16-bit output only. Issue #22 records the exact next
discriminator; no image patch or forced 15-bit mode is justified.

The `99a42aa3` capability run's continuing 24/15-bit toggles add no independent image evidence: it
captured neither raw VRAM nor a matched final present/software control, so Issue #22 remains
investigating.

### S008 — Authored projection publication

The identity-checked executable derives SetGeomOffset `0x80083CD4`, SetGeomScreen `0x80083CF4`, and
graphics-init values `256/120/160`. Static, hermetic, and live checks show the narrow HLE boundary
preserves guest effects and publishes valid projection state on the same Core.

Gap: remaining projection/culling writers are unmeasured, and no widescreen, interpolation, or native
producer consumes the published state yet.

### S009 — Native scene producers

Missing capability: the framework has a native rendering route, but Toy Story 2 has no title-owned
camera, world, actor, effect, or 2D producer feeding it from game state. Guest GP0 packets remain the
current picture source. `ToyStory2Runtime` explicitly declares the intended
`RenderCapabilities::interpolatedNative()` product profile so Native and 60fps remain available while
those title-owned producers are built; that declaration is not evidence that the missing producers
already exist.

The `99a42aa3` product log resolves `PSXPORT_RENDER_PATH=native` and reports the Native render path.
That proves the requested route was accepted, not that Toy Story 2 supplied any of the missing scene
producers.

### S010 — True widescreen

Missing capability: there is no title-owned aspect policy, expanded visibility/culling, edge coverage,
or UI anchoring implementation, and no matched 4:3/16:9 title and gameplay evidence.

The `99a42aa3` capability run does not change this state. `PSXPORT_WIDE=1` was reported UNKNOWN and
did nothing, while the loaded configuration contained no aspect setting. Shipping widescreen is
selected by `PSXPORT_SETTINGS` pointing at an INI with `aspect=1`; Issue #24 owns the missing controlled
invocation and direct wide-projection evidence.

### S011 — Interpolated 60fps presentation

Missing capability: no stable previous/current authored camera, object, or effect state is captured
and consumed between simulation ticks. The current guest field commit deliberately adds no temporal
decoration.

The `99a42aa3` log reports `[fps60] TRUE per-object interpolated 60fps ON (source: env)`. This proves
the fps60 request was accepted by a title declaring temporal interpolation; without authored
previous/current producer state or visual evidence, it does not prove interpolated presentation.

### S012 — `.RAW` asset decoding

Evidence: `tools/raw_probe.py` verifies framing and packed CRCs; `tools/raw_unpack.py` implements the
Traveller's Tales `DecompressRAW` LZ scheme and verifies both CRCs for 813/813 chunks across 46 files.
Independent LEVEL01 extractions byte-match all 39 decoded chunks.

### S013 — Fresh-clone product launcher

The three-line `run.sh` enters the frozen uv environment; `bootstrap.py` and `tools/run.py` own disc
resolution, 22-module provisioning, generated refresh, isolated player build, dependency refusal, and
windowed launch. Injected-host tests cover the zero-argument and cold prepare-only contracts. The clean
Clang consumer build against exact psxport `99a42aa3` passes all 15 CTest gates, including the generated
CPU boundary, projection boundary, and full C++ policy.

An exact configured product run resolved the Native render path and fps60 environment request, remained
CPU-active for more than five minutes, and advanced disc/display state without a watchdog or fatal
diagnostic. It was not a launcher-completion result: `PSXPORT_NATIVE_FRAMES=2200` was UNKNOWN at startup
and did not terminate the actual product, so the operator stopped exact PID 1972962 through `safekill`
(exit 137). Evidence is `scratch/logs/capabilities-product-99a42aa3.log` plus the operator's PID/CPU
observation. Issue #23 owns the missing reliable product-level bound.

Gap: the zero-argument windowed product still needs direct user validation; each framework pin bump
requires the combined consumer gate before the recorded pin becomes current evidence; and automated
product evidence is not safely frame-bounded until Issue #23 is resolved.
