# Project state

Factual capability coverage for the Toy Story 2 port. Epic intent lives in
`docs/project-goals.md`, atomic work in `docs/issues/`, ownership in `docs/codemap.md`, and ordered
binary evidence in `docs/re-frontier.md`.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | USA executable, disc files, and loaded code modules are reproducibly identified and placed | verified | — | G001 |
| S002 | Retail resident and overlay code is reproducibly recompiled into the shipping product | partial | S001 | G001 |
| S003 | The host owns finite title frame iteration, field timing, input, audio, and one presentation commit without guest VSync | partial | S002 | G001 |
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

S009 is the current focus: decode the now-live dominant mesh stream into a visible native producer
under issue #30.

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

### S003 — Host-owned finite frame iteration

The title runtime supplies the shared shell's required `FrameDriver`. Its first measured slice replaces
two-field wait `0x8003FA68` with host field ticks, routes deferred display work through `0x80021028`
instead of dispatching guest VBlank `0x80039D60`, advances input/audio, and closes exactly one neutral
presentation fence. Transition/front-end frames own one field and resident frames own the measured two.
A finite state owner now performs cold front-end setup, one
event poll or interactive-selection iteration per frame, resident preparation, and dispatches normal
`0x8007B254` or alternate `0x8007B850` according to measured RAM state. MEMORY/display initialization
starts inside the first finite frame. Graphics initialization `0x8003A218` retains its measured buffer
effects through a title-local runtime override but omits its two VSync calls; generated supers are
unchanged. Linked libetc VSync `0x80088628` remains fatal in its exact second HLE window, and the retired
`host_turn` field clock remains absent. Successive real launches used that trap to move stock-libcd IRQ
timeouts to the synchronous CD owner, then replace libpad's VBlank-driven boot/decode with a measured
native digital-packet owner. A later launch crossed both plus the libgpu timeout arm/check owner and
wrote two presents. Both captures were black, then the title retried absent `/LEVEL00/LEVEL.DAT;1`
until the watchdog fired. Issue #28 traces that path to the finite driver dropping the selected level
live in `a0` at retail call site `0x8007AE08`, forcing level zero. Bounded re20 crossed that retry,
proving the corrected ABI, then reached the next fatal guest VSync in 0x80039D9C through blocking
pre-resident fade 0x8007C344. Its only new presents, 1 and 2, were fully black.

The current frame-owner source replaces the synchronous 0x8007BEC4/0x8007C344 route with a finite resident-
preparation owner which consumes one authored transition field per frame, then invokes a state-only
512x240 graphics initializer without either guest VSync. Bounded retail re21 (PID 2678458) crossed
that chain, exited itself at the explicit 300-frame cap, reconciled 300/300 frames with zero dropped
layers, and hit no VSync trap, fatal, miss, unmapped access, or watchdog. Presents 45 through 300 show
coherent Andy's Room demo gameplay with changing camera, Buzz, enemies, effects, and HUD. The finite
boundary now passes 11/11 (83 checks), frame-fence ownership passes 19/19, and clang-tidy is clean.
After the packet-pool facts and current shared framework were rebuilt with Clang, bounded retail re35
(PID 2991368) independently exited at 120/120 reconciled frames with zero dropped layers and no VSync
trap or fatal; its four inspected captures again show coherent Andy's Room demo gameplay.

The next source slice transcribes the measured post-resident predicate, cleanup, demo/sequence exits,
and transitions back to cold front end, warm front end, resident setup, or shutdown. Graphics shutdown
also retains synchronous drain/callback/reset state while omitting its guest waits. These paths build,
but product execution has not reached them yet.

Gap: this is not yet a complete product loop. The independent MEMORY and FMV owners are issue #26 and
issue #27 under the encompassing issue #25. Re21 verifies the resident demo/gameplay route, not those
alternative overlay loops or indefinite interactive play.

### S004 — Boot through gameplay

The generated product renders legal and ESRB screens, enters the FMV module and resident renderer,
reaches a stable title, loads LEVEL01, clears and reloads the measured model slot, and has reached
Andy's Room under bounded host input. The current native FrameDriver independently completes its
explicit 300-frame real-disc bound and shows coherent Andy's Room demo gameplay from presents 45
through 300 without guest VSync, fatal, recompilation miss, unmapped access, watchdog, or dropped
presentation layer.

An exact-`99a42aa3` capability run also stayed CPU-active for more than five minutes, advanced disc
activity through LBA 15660, and continued toggling between 24-bit and 15-bit display modes without a
watchdog or fatal diagnostic. The operator stopped its exact PID after the requested frame budget
failed to terminate it; this is sustained-progress evidence, not a successful bounded-run result.

Gap: current FrameDriver evidence covers the resident demo/gameplay route, not the independent MEMORY
and FMV loops, post-resident transitions under live execution, or indefinite interactive play. Issue
#25 owns those remaining loop routes.

### S005 — Repeatable player control

The retail pad initializer and independent consumer derive both guest packet destinations. A bounded
host pulse writes active-low Cross and release bytes to the measured slot, and a live run has paused,
unpaused, and moved the camera.

Gap: replaying the exact recorded pad samples diverges at the later pause transition. Issue #20 owns
the deterministic replay investigation.

### S006 — Coherent guest-rendered presentation

The retired guest-owned field loop committed through the neutral shared fence, and its captured legal,
ESRB, title, and gameplay frames remain historical 15-bit evidence. The current native FrameDriver now
has an independent 300-frame visual witness: the 4:3 re21 contact sheet shows coherent Andy's Room
world geometry, characters, effects, and HUD while the presentation ledger reconciles 300/300 frames
with zero dropped layers. Current-binary re35 separately reconciles 120/120 frames with zero dropped
layers; inspected presents 45, 60, 90, and 120 show the same coherent room, animated characters,
effects, and HUD.

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
world, actor, effect, or 2D producer feeding it from game state. Guest GP0 packets remain the current
picture source. The first authored input owner now captures stable previous/current camera position
and wrap-aware 12-bit rotations after each resident update; real-disc re37 confirms those values
change alongside coherent gameplay. The runtime also has a source-verified observation seam for exact
`0x8002622C` list/count batches and actual
`0x800100E4` mesh-call arguments. Both runtime wrappers super-call the generated bodies. Diagnostic
real-disc re39 reaches the seam across 89 resident updates: four batches per update, changing
candidate totals 142..198 and mesh-call totals 66..86. It exits at 120/120 reconciled frames with zero
dropped layers and preserves coherent distinct Andy's Room frames 60 and 120 without guest VSync or
fatal. Because psxport changed during the active Clang build, re39 is live producer evidence but not a
final clean product certification. The subsequent clean Clang build against landed psxport
`3c342ec3` passes 20/20 CTests. Exact-product re40 (PID 3160059) reproduces the same 89 updates and
ranges, exits at 120/120 with zero dropped layers and no guest VSync/fatal, and preserves distinct
coherent frames 60/120. A validated identical-frame control reports zero changed pixels; re40 frames
60 to 120 change 671,652/691,200 pixels. A title-owned checked decoder now derives the dominant
submitter's signed header, base XYZ/colour vertices, command offsets, opcode-specific descriptor
strides, packed vertex indices, packed texture-coordinate selection, material/blend command fields,
and terminators entirely from source RAM before GTE/OT/GP0 output. It also summarizes complete
command streams for the next authorized live opcode/material census. Texture-resource resolution and
2D submitters remain ungrounded, and
there is still no native draw. This remains producer input, not a visible native layer. The runtime
therefore remains `RenderCapabilities::widescreenOnly()`: GTE is the default product picture and
Native/temporal interpolation stay unavailable until real title producers consume the state. The
earlier `interpolatedNative()` declaration was false exposure, not a working renderer.

### S010 — True widescreen

Missing capability: the controlled `aspect=1` real-disc run completes 300 native-owned frames with no
guest VSync or dropped layer, but its player-facing Andy's Room captures contain large vertical black
slabs. The proposed 684-pixel guest draw area crosses the fixed 512-pixel horizontal parity inside
1024-pixel VRAM and exposes wrapped/atlas columns. The arithmetic boundary passed while the actual
picture failed, so none of its widescreen outputs are accepted evidence. Issue #24 owns the falsified
candidate and exact captures. `PSXPORT_WIDE=1` remains an unsupported no-op.

### S011 — Interpolated 60fps presentation

Missing capability: stable previous/current authored camera position and wrap-aware rotation are now
captured between simulation ticks. The source also retains bounded previous/current visibility and
mesh-entry snapshots, but it has no identity-matched object interpolation and no visible native
consumer. The runtime correctly withholds temporal interpolation,
and the title frame owner deliberately uses a neutral commit. Passing the decorator now would only
duplicate the captured guest queue at 60Hz, not lerp Toy Story 2 motion, so it remains disconnected
until real producer state exists.

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
windowed launch. `-h` and `--help` print usage and exit 0 before dependency, framework, executable, or
disc discovery; the launcher contract forces every discovery seam to remain untouched for both
spellings. The product executable handles the same flags before installing the runtime or constructing
a `Game`; two CTest contracts supply an impossible disc path. Injected-host tests also cover the
zero-argument and cold prepare-only contracts. The clean Clang consumer build against exact psxport
`3c342ec3` passes all 20 CTest gates, including the generated CPU boundary, native frame/CD
boundaries, VSync ownership, both direct help flags, and full C++ policy.

An exact configured product run resolved the Native render path and fps60 environment request, remained
CPU-active for more than five minutes, and advanced disc/display state without a watchdog or fatal
diagnostic. It was not a launcher-completion result: `PSXPORT_NATIVE_FRAMES=2200` was UNKNOWN at startup
and did not terminate the actual product, so the operator stopped exact PID 1972962 through `safekill`
(exit 137). Evidence is `scratch/logs/capabilities-product-99a42aa3.log` plus the operator's PID/CPU
observation. Issue #23 owns the missing reliable product-level bound.

Gap: the zero-argument windowed product still needs direct user validation; each framework pin bump
requires the combined consumer gate before the recorded pin becomes current evidence; and automated
product evidence is not safely frame-bounded until Issue #23 is resolved.
