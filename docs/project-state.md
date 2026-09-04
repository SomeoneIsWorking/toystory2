# Project state

Factual capability coverage for the Toy Story 2 port. Epic intent lives in
`docs/project-goals.md`, atomic work in `docs/issues/`, ownership in `docs/codemap.md`, and binary
evidence in `docs/re-frontier.md`.

## Comparison baseline

The baseline is the unmodified USA PlayStation release `SLUS_008.93` running in a general-purpose
emulator. Intended differences are a standalone native/dynarec product, host-owned finite frame
iteration, native input and render ownership, true widescreen, and interpolated presentation.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | USA executable, disc files, and loaded modules are reproducibly identified and placed | verified | — | G001 |
| S002 | psxport executes resident and streamed code through a gameplay dynarec with no interpreter fallback | blocked | S001 | G001 |
| S003 | Native title owners provide finite frame, timing, input, audio, and presentation sequencing | partial | S002 | G001 |
| S004 | The current product boots through front end and LEVEL01 gameplay | blocked | S002, S003 | G001 |
| S005 | Host input produces repeatable guest gameplay behavior | partial | S003, S004 | G001 |
| S006 | Guest-rendered 15-bit screens and gameplay present coherently | blocked | S002, S003 | G001 |
| S007 | Toy Story 2's 24-bit MDEC movies present coherently | partial | S002, S003 | G001 |
| S008 | Authored projection is published to title-owned consumers | partial | S002 | G002, G003 |
| S009 | Visible scene layers have native game-state producers | missing | S008 | G002, G003 |
| S010 | True widescreen composes correct title and gameplay pictures | missing | S008, S009 | G002 |
| S011 | Presentation interpolates stable authored state at a player-facing 60fps cadence | missing | S008, S009 | G003 |
| S012 | Traveller's Tales `.RAW` assets are reproducibly framed and decompressed | verified | S001 | G001 |
| S013 | The fresh-clone launcher builds and starts the intended product | partial | S001, S002 | G001 |

## Current focus

S002 is the current focus. Issue #31 tracks the only permitted product blocker: psxport's maintained
dynarec-only Lightrec dependency is not linked yet.

## Capability details

### S001 — Reproducible retail inputs and module placement

Evidence: `tools/extract_exe.py` identifies the 598,016-byte executable; exact identity is recorded in
`docs/info/exe-identity.txt`. `tools/overlay_map.py` derives the LEVEL slot at `0x800D12C0`, the shared
MEMORY/FMV slot at `0x800D5D20`, and FMV entry `0x800D6628`. The disc census identifies 22 loaded code
modules without tracking game bytes.

### S002 — Dynarec-only guest execution

The obsolete offline translator, emitted source corpus, seed manifest, generated registry, product
selector, and static-only tests are absent. Title guest calls and native overrides use psxport's typed
runtime APIs.

Blocker: psxport's executor currently returns the named `Lightrec dynarec-only backend is not linked`
fault. Issue #31 owns integration of a maintained Lightrec fork that cannot interpret difficult
blocks. No gameplay fallback is present.

### S003 — Native finite frame ownership

The retained title modules own the measured front-end/resident state machine, field quota, native pad
packets, deferred display service, audio step, and one presentation commit without guest VSync. Their
hermetic boundary tests predate the execution migration and the sources now use the typed dynarec
guest-call adapter.

Gap: runtime behavior has not been reverified through Lightrec because S002 is blocked. MEMORY and
FMV loop ownership also remain incomplete under issues #26 and #27.

### S004 — Current boot through gameplay

Blocker: S002. Earlier execution evidence reached coherent Andy's Room, but it used the removed
executor and is only a scenario expectation, not evidence for the current product.

### S005 — Repeatable player control

The exact retail pad buffers and native active-low packet producer remain implemented. Earlier runs
paused, unpaused, and moved the camera, while exact sample replay diverged at a later pause transition.

Gap: the scenario must be reverified through the current dynarec product after S002 and S004.

### S006 — Coherent 15-bit presentation

Blocker: S002. The guest GPU/presentation path and native frame owner remain, but no current product
frame has been produced through Lightrec.

### S007 — Coherent 24-bit MDEC movies

Gap: the MDEC path reaches 24-bit mode in retained evidence, but the captured frame had duplicated noisy
columns and was mostly black. Issue #22 owns the independent raw-VRAM versus reference discriminator.
The current product must first cross S002 before that fault can be retested.

### S008 — Authored projection publication

Exact executable evidence derives projection leaves `0x80083CD4` and `0x80083CF4` and initialization
values `256/120/160`. The title retains its hermetic publication boundary and runtime native owner.

Gap: current live reach and remaining culling/projection writers need verification through S002.

### S009 — Native scene producers

Missing capability: title code captures authored camera, visibility-list, instance, and mesh-command
inputs, but no title-owned world, actor, effect, or 2D producer emits the visible picture. Issue #30
owns the semantic producer boundary.

### S010 — True widescreen

Missing capability: expanding the guest draw canvas crosses fixed VRAM parity and exposes invalid
columns. Correct widescreen requires a semantic native producer plus projection, viewport/scissor,
culling, and 2D layout ownership. Stretching and frame sampling are excluded.

### S011 — Interpolated 60fps presentation

Missing capability: camera history exists, but there is no identity-matched object history or visible
native consumer. Interpolation remains unavailable until S009.

### S012 — `.RAW` asset decoding

Evidence: `tools/raw_probe.py` verifies framing and packed CRCs; `tools/raw_unpack.py` implements the
Traveller's Tales LZ scheme and verifies both CRCs for 813/813 chunks across 46 files. Independent
LEVEL01 extractions match all 39 decoded chunks.

### S013 — Fresh-clone product launcher

The three-line `run.sh` enters the frozen uv environment. `tools/run.py` resolves psxport, configures
the shared top-level `build/player` tree, builds `build/player/bin/toystory2_port`, and launches only
that target. Its injected-host suite passes the help, dependency refusal, compiler portability,
explicit disc, build-only, and zero-argument paths.

Gap: the default product cannot enter gameplay until S002, and the cold real build/launch path has not
been rerun for this migration.
