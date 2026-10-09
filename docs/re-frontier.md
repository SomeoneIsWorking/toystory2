# RE frontier

This is the ordered evidence dependency chain for exact USA `SLUS_008.93`. Static analysis produces
symbols and facts only; runtime execution belongs to psxport's dynarec and never to emitted guest
source. Historical execution observations remain scenario inputs but do not certify the current
runtime.

## tooling

### RE-00 — reproducible Ghidra project
- status: re-verified
- deps:
- evidence: C008 and I005. Header-driven RAM placement imports 595,968 executable bytes at `[0x80010000,0x800A1800)`, and the xref selftest proves positive and negative folds.
- where: `tools/ram_image.py`; `tools/ghidra_xref.py`; `tools/re_xref.py`; psxport's Ghidra utilities
- gap: Function boundaries and names remain hypotheses until each downstream step verifies them.
- notes: Derived databases and decompilations stay in gitignored `scratch/`.

## boot

### RE-01 — crt0 and boot layout
- status: re-verified
- deps: RE-00
- evidence: C009 and I008. The identity-checked PS-X EXE yields 43 walked retail instructions and derives BSS `[0x800A1070,0x800D12C0)`, stack `0x80200000`, heap base `0x800D12C0`, heap size `0x126D40`, gp `0x800A0CD8`, libc init `0x80089344`, game main `0x8007A9E8`, and entry `0x80082D60`.
- where: `game/facts/guest_facts.h`; `tools/extract_exe.py`
- gap: none for the exact boot group
- notes: The verifier consumes psxport's neutral PS-X EXE parser, not an execution generator.

### RE-02 — dynarec-only execution boundary
- status: in-progress
- deps: RE-01, RE-03
- evidence: The title build and source route guest calls and image-scoped overrides through `game/execution/guest_execution.*`; No offline translator, generated corpus or static dispatcher exists in this tree.
- where: `game/execution/guest_execution.*`; `external/psxport/runtime/cpu/`
- gap: The backend links and the exact resident `0x80039B74` contains finite table loops; the retail graphics initializer returns across two bounded Lightrec slices. Title-owned MEMORY byte authentication and code-image publication pass synthetic tests and were reached by exact-revision retail execution, including replacement. An asset decode that reached a `CdRead` refusal without Setloc and exited a strict guest call as budget-exhausted at `0x800940F4` has an unproven causal relationship with that refusal. See S002 in `docs/project-state.md`.
- notes: Do not restore an offline translator, generated corpus, static dispatcher, engine selector, or unbounded interpreter fallback.

### RE-11 — FMV path parser and shared BIOS toupper
- status: re-verified
- deps: RE-03
- evidence: C016. Retail FMV call `0x800D8E48` reaches executable Sony leaf `0x80082E5C`, selects BIOS A0:0x25, and stores the normalized byte at return `0x800D8E50`.
- where: shared implementation belongs in psxport
- gap: A changed executable leaf, selector, or parser return reopens the result.
- notes: Live progress must be reverified through RE-02.

### RE-12 — resident renderer computed jump table
- status: re-verified
- deps: RE-00
- evidence: C017. Exact retail words build table base `0x800103EC`, mask the selector with 31, scale by eight, and use `jr` through 32 consecutive `j` plus delay-slot trampolines inside `0x800100E4`.
- where: exact binary/Ghidra evidence recorded by C017
- gap: none for classification and table shape; dynarec support is owned by psxport
- notes: An address-specific override is not a substitute for correct indirect-control-flow lowering.

### RE-14 — LEVEL01 entry `0x800D12C4`
- status: re-verified
- deps: RE-03
- evidence: C020. The module's first word is its ID; file+4 is a valid prologue, the retail loader places matching bytes at the slot, resident code directly calls it, and five descriptors store it as their entry.
- where: binary and runtime image evidence recorded by C020
- gap: none for entry classification
- notes: Register this only as runtime module identity; it is not a compile seed.

### RE-16 — model-table reset delay-slot semantics
- status: re-verified
- deps: RE-00
- evidence: C021. Retail `0x80041F38` has a branch at `0x80041FF8` whose delay slot `0x80041FFC` advances the model-table cursor. The exact instruction fact explains the former repeated slot-zero clear.
- where: binary/Ghidra evidence recorded by C021
- gap: The behavior must be covered by shared MIPS delay-slot tests in the dynarec before the old gameplay scenario is considered recovered.
- notes: Fix the shared lowering rule, never this address.

### RE-17 — first bounded interactive dynarec witness
- status: todo
- deps: RE-02, RE-06, RE-13
- evidence: Earlier runtime scenarios reached Andy's Room, pause/unpause, and camera motion, but used the removed executor and are expectations only.
- where: future dynarec differential/playthrough evidence
- gap: Reproduce gameplay entry, pause/unpause, held-camera movement, streamed-module replacement, audio and sustained progression through Lightrec with explicit translated and bounded-fallback denominators.
- notes: Instrument reach and input delivery denominators before interpreting absence of a symptom.

## overlays

### RE-03 — overlay load bases and runtime identities
- status: re-verified
- deps: RE-00
- evidence: C010, C014 and I009. LEVEL alternatives load at `0x800D12C0`; MEMORY and FMV reuse `0x800D5D20`; FMV entry `0x800D6628` is file+`0x908` and begins prologue word `0x27BDFF10`. The corpus contains 22 code modules.
- where: `game/facts/guest_facts.h`
- gap: none for the proven module set and two reused physical slots
- notes: Runtime identity is authenticated image, generation, and address; address alone is ambiguous.

## cd

### RE-04 — CD load chokepoints
- status: re-verified
- deps: RE-01
- evidence: C010, C012 and the whole-file read decompiled. The chain is `0x80082508` (normalize, upper case, strip the ISO9660 version suffix) → `0x80082728` (clear `DAT_800A15A8`, then two `do { } while` loops around the read) → `0x80082608` (clear `DAT_800A1034`/`DAT_800A1588`, `CdInit(0xB,0,0)` at `0x80090D40`, spin on `CdSearchFile` `0x80092AE8`, `CdRead` `0x80093AF0`, spin on `0x80093BF4`, return the `CdlFILE` size or 1 for an empty file). `0x80082608` has exactly one caller (`0x80082750`), so the two paths are one path. `0x80082648` and both `0x80082750`/`0x8008276C` loops are the design's three spin sites and all three now sit inside native owners. `0x8007F174` is the fourth, in the file *processor*: decompiled whole, the delay slot at `0x8007F170` stores zero into `$s0` and the branch at `0x8007F180` is `beqz $s0`, so `do { VSync(0); break 1; } while (true)` at `0x8007F174` is the routine's own assertion that the searched bank fits the window `(v + 0x7FF) & ~0x7FF < 0x4001` it accepts — unbounded in retail too, and its sibling legs report through the game's own two messages and return.
- where: `game/cd/file_transfer.h` (`ts2::cd::FileTransfer`, registered at `0x80082608`, and the bounded retry policy at `0x80082728`); `game/audio/sound_bank.h` (`ts2::audio`, registered at `0x8007F108`, the guest's own body run through the framework's bounded resume loop so the assertion costs bounded fields and ends as a named refusal); `game/cd/stock_libcd_layout.h`
- gap: the play loop's entry state (the eleven stores at `0x8007AE20`, the exit countdown and `[0x800A1430] = [0x800C166C] << 1`, the fade) now runs as the guest's own continuation rather than a replay, so a payload comparison against retail is available as evidence for the level start itself.
- notes: Do not special-case a BIOS call or fabricate load completion. The native read keeps the guest's CD-mode publication (`0x80090D40(0xB,0,0)`) so the parts of the product that still stream through the guest's CD keep working.

## frame and input

### RE-05 — authored render-source boundary
- status: re-partial
- deps: RE-01
- evidence: Resident buffers are `0x801BBD28` and `0x801DD21C`; packet pools begin at `0x801BBFEC` and `0x801DD4E0`. Camera producer `0x8002C848`, scene root `0x8002A070`, visibility lists `0x800BB4D8`/`0x800C0AB0`, owner `0x8002622C`, and mesh submitter `0x800100E4` are grounded from binary and earlier reached observations. The mesh submission WINDOW is the guest's own screen rectangle: `SetScreenRect` at `0x80010000` (10 instructions, stores `$a0..$a3 << 16` to `0x1F800060/64/68/6C`), read back by its twin `0x8001002C`, with four publishers — renderer `0x8002A404` and `0x8002A6C0`, submitter `0x8002638C`, second submitter `0x80026E84`.
- evidence (record path):
  - Frame timing: the OT built in update N is drawn by the deferred field service `0x80021028` → `0x8003A888` during frame N+1. `0x8003A888` then swaps the buffers and resets the OT `gp+0x930 = buf+0x1924` and the pool `gp+0x3E4 = buf+0x2C4`.
  - Slot scheme of the static submitter `0x800100E4`: each instance's per-face table is published at `*0x800A11CC` (u16 per entry, 0 = none). The packet is `slot*4 + *0x1F800044`, the current buffer's slot array, with a copy at `*0x1F800048`. A face keeps its slot while it stays visible, and the guest relinks that packet every frame. Opcodes 8..15 take two entries per primitive.
  - The rigid drawer `0x80017FF8` uses the same scheme with one entry per primitive, positive headers only.
  - Both bodies are ported (`game/render/rigid_mesh_drawer.*`, `static_mesh_drawer.*`, `mesh_cpu.h`). Static dispatch table `0x800103EC`: ops 0/4 `0x80014838`, 1/5 `0x80013F20`, 2/6 `0x80014354`, 3/7 `0x80013AFC`, 8/12 `0x80012FE0`, 9/13 `0x80011BD8`, 10/14 `0x800124E8`, 11/15 `0x800112D8`, 16/20 `0x800104EC`, 17/21 `0x80010C64`, 18/22 `0x800108B8`, 19/23 `0x80010F8C`, >=24 exit `0x800112A8`; rigid jump table `0x80018114`. Subdividers: quad textured `0x80014D3C`, triangle textured `0x80016690`, quad plain `0x8001A4D4`, triangle plain `0x8001B8AC`; release helpers `0x80017B34/6C`, `0x80017ECC/F04` use the scratch software stack at `0x1F800360`.
  - Reached on the player route (2,500 frames, handler entries): ops 0 238,811, 1 91,652, 2 1,549, 4 97,093, 16 48,165, 17 18,728, 20 15,217, all ported and override-diff clean. Ops 3, 5-7, 9-11, 13-15, 18, 19 and 21-23 are not reached by any replay; they take the guest path through `MeshDrawers::plan` (no native handler). Op 12 is reached only through the 2,342 negative-header meshes per route, which are unported.
  - A negative header (`<= 0`) means auxiliary vertex records: a prologue normalises rotation rows 1-3 (`0x80083944`), writes light control registers 16..20, clears the cache at `0x800B27A8`, then command 0 and 12, terminal 31. Not ported (issue 41).
  - The rigid and static column-cull branches carry a depth-average GTE op (AVSZ) in the branch delay slot, so OTZ is written for culled primitives too.
  - Faces flagged at packet `+0x27` are subdivided through `0x80017B34` → `0x80017B6C`, recursive, with sub-packets from a sequential cursor. On the Andy's House route over 360 frames no slot was claimed twice in a frame and no face changed slot.
  - The object renderer `0x8002518C` and the hierarchical drawers `0x8002B6F0`/`0x8002AC40` allocate packets one after another from the pool cursor `0x800A1608` and carry no per-face identity (issue 40).
- where: `game/render/slot_mesh_producers.*`; `game/render/mesh_format.*`; `game/widescreen/resident_widescreen.*`; `game/frame/frame_cut.*`
- gap: The object-renderer drawers and the subdivider have no key (issues 40, 41). The per-object screen box the submitter publishes is produced by the visibility leaf `0x80027AF0`, which clamps each projected corner to the console frame (`slti $v0,$v0,0x200` at `0x800280D4`, `$s6 = 0x200` stored at `0x800280F8`) BEFORE intersecting with the caller's rectangle, so that leaf's rectangle argument alone cannot keep a margin object submitted; the publisher at `0x80010000` is the seam the widened frame has to cross. That clamp is now bypassed for the widened frame, so what remains visible in the right margin is measured scene content: the under-bed void is `(0,0,0)` inside the console frame at 4:3 too, and the region past the floor mesh's own edge samples the `(33,33,33)` backdrop, which is what "no prim covers this pixel" rasterizes to once the only horizontal clip in the chain is open. The camera producer `0x8002C848` is the first native producer, and its projection work is `0x80083B44`, a 67-instruction COP2 transform that runs `RTPS` on one quad and writes the transformed 4x3 set plus the screen length at data register 17; `0x8002C848` calls it four times.
- notes: On the record path the guest's GP0 is the picture. Widescreen comes from the canvas plus the widened culls. Interpolation comes from keyed producers.

### RE-05b — scene and camera cuts
- status: re-partial
- deps: RE-05
- evidence:
  - The camera object `0x800C1540` is set by the camera controller `0x80068E2C` from the follow camera `0x800BA048`, or from the look camera `0x800B2228` when bit 0 of `0x800B2256` is set. It is copied directly when the blend counter `0x800A1498 < 1` and blended otherwise.
  - Only `0x800646A8` (starts the look camera, counter `0x40`, called from `0x8007678C` after the camera update), `0x80074F80` (end of update) and `0x80068E2C` write the counter.
  - `0x80065CD0` zeroes the camera and places it afresh. It is called from level start `0x8007BEC4` and respawn `0x8007BCE4`.
  - The area object table `0x800A1274` is written only by `0x8002EFB4`, called only from the asset loader `0x8003D88C`. The level id is `0x800A16A8`.
  - On the Andy's House route the cuts are boot, the front-end transitions, the level load and the level-start camera placement.
- where: `game/frame/frame_cut.*`; `tests/resident_producers_boundary.cpp`
- gap: Respawn and the look-camera switch are proven only through the boundary test, not on a live route.

### RE-05a — the guest camera producer
- status: re-partial
- deps: RE-05
- evidence: `0x8002C848` is 1273 instructions spanning `0x8002C848..0x8002DC2B`, reached only from `0x8007B254` / `0x8007B850` (the two resident-update owners) which `0x8007A9E8` (guest main) calls; the title enters the resident update directly, so the producer does run in play. Its camera section is `0x8002C848..0x8002CA4C` and its light field `0x8002CA50..0x8002DC2B`. The layout below was verified against the guest with psxport's own per-call override differential (`PSXPORT_OVERRIDE_DIFF=0x8002C848`), which restores the original's state so the run stays faithful: the three angles at camera+`0x0C`/`0x0E`/`0x10` with the complement `0x1000 - pitch` masked to 12 bits; published angles `0x800A1618` (three halfwords) and retained `0x800A15C8`; publish flag `0x800A1344`; previous-tick copies `0x800CEA48` (32 bytes from `0x1F800394`) and `0x800CD498` (16 bytes from `0x1F800374`); the four phase-offset words `0x800A1160`/`0x800A11A4`/`0x800A11B8`/`0x800A13CC`; the camera rotation `0x800B2258`; the four light matrices `0x800A8360`/`0x800A8380`/`0x800A83A0`/`0x800A83C0`; the scroll ring `0x800CD4A8` (64 halfwords) and the light ring `0x800CD5F0` (64 four-halfword records). The GTE commands are the retail words `GPF` `0x4B98003D` and `RTPS` `0x486012`, and the guest's COP2 sub-opcodes are Beetle's table (`MFC2` 0, `CFC2` 2, `MTC2` 4, `CTC2` 6).
  Facts that corrected earlier readings, each found by the differential: the third angle's table index is the angle itself masked to `0xFFF`, NOT doubled like the first two; the cosine table is `0x800` BYTES after the sine table at `0x80097E48`; every table index is a HALFWORD index, so the byte offset is twice it; the `1.6` stretch reaches only the first THREE rotation halfwords, not all ten; `0x800A1208` holds a POINTER to the byte table rather than being one; the scale chain is 1172 with a `bgez` that skips the `+ 0xFFF` for a negative product, i.e. it rounds toward zero; the routine RETURNS the address of its last light matrix in `v0`; the retained third angle at `0x800A15CC` is NOT the third angle — the guest copies its own frame word at `$sp-0x7C` into that slot BEFORE storing the angle there; and the light-matrix blend has NO single per-halfword form. That last one is the substantive finding: all four blocks write nine of their ten halfwords in the SAME irregular order, and the forms are `h0` secondary weight with the magnitude folded in before the multiply, `h1`/`h2` secondary weight, `h3` NO weight, `h4` no weight with the magnitude added after the shift, `h5` no weight, `h6`/`h7` primary weight, `h8` primary weight with the magnitude folded in before the multiply, with `h9` never written at all. Assuming one form for the whole matrix is what produced the frozen grey field.
- gap: the producer is NOT installed. A full transcription still leaves 97 bytes and 11 registers differing per call, and the 97 bytes are real. The 11 registers are `v1` and the callee-saved `s0`-`s8`, which the caller restores from its own frame and so cannot affect behaviour. Since a camera that differs from retail is a fidelity regression, S011 reads the camera the GUEST publishes at `0x1F800374`/`0x1F800394` rather than owning a reimplementation of its producer.
- notes: This entry describes the retired Gte-path approach; the record path does not re-project. THE WORLD PASS HAS NO RE-RUNNABLE PRODUCER, which is what blocks S011's world half and is recorded in S011: `RqItem` carries screen verts, sub-pixel XY and per-vertex depth and no view matrix, so a captured guest submission cannot be re-projected; and the guest's world pass `FUN_8002a070` (679 insns, no args) is one interleaved routine that loads the camera into the GTE (`FUN_80083c84(&DAT_1f800394)`), composes the per-view matrix (`FUN_8001e8d4(&DAT_800a1618,&DAT_1f8003b4)`), allocates the merge arrays (`FUN_80027724`), culls (`FUN_8002622c`), and appends every packet while advancing the guest's own OT write cursor `DAT_800a151c` — so re-running it at present time appends a second world to the guest's OT. Guest OT world items are `RQ_WORLD` with `has_xyf == 0` because `emitOrQueue` zeroes every item first, which is exactly the predicate `LegacyTemporalSceneSource::owns()` rejects; the only producer that sets `has_xyf` is `RenderQueue::drawWorldQuad`, i.e. a native world producer this title does not have. `tools/headless_run.py` runs `build/verify/bin/`, NOT `build/bin/`; comparing against the wrong build tree makes every A/B vacuous. A probe that logs to an unregistered channel is silently invisible in `scratch/headless/run.log`. The per-call differential is far better than a title-local probe for this and costs one env var, but it reports only the FIRST difference per sampled call, so a residual of a dozen scattered words costs one six-minute run per step; prefer sampling a later call with `PSXPORT_OVERRIDE_DIFF_FIRST=0` / `_EVERY=500` when the early-boot state is not what is being measured.

### RE-06 — pad driver buffers
- status: re-verified
- deps: RE-01
- evidence: I018 derives buffers `0x800CF8A0`/`0x800CF8C8`, driver pointers `0x800A3E98`/`0x800A3F88`, `0xF0` stride, and consumer `0x8003AC58`. The verifier exercises active-low Cross and release.
- where: `game/input/pad_owner.*`; `tests/frame_turn_boundary.cpp`
- gap: End-to-end response through the current dynarec belongs to RE-17.
- notes: Native input writes the measured packet; it does not emulate an unrelated SIO protocol locally.

### RE-07 — projection publication
- status: re-partial
- deps: RE-01
- evidence: C023 and I019 derive SetGeomOffset `0x80083CD4`, SetGeomScreen `0x80083CF4`, and authored initialization `256/120/160`. The hermetic title boundary checks guest and host effects.
- where: `tests/toystory2_projection_boundary.cpp`; `game/widescreen/guest_widescreen.*`
- gap: Remaining projection/culling writers and current live reach are unverified. One further culling writer is now identified and owned: the screen rectangle the mesh submitter publishes (`RE-05`), which the visibility leaf's console-frame clamp truncates at column 512.
- notes: Publication does not itself implement widescreen.

### RE-10 — title field timing ownership
- status: re-verified
- deps: RE-01
- evidence: Retail VBlank callback `0x80039D60` advances the state consumed by wait `0x8003FA68`. The native frame owner instead supplies the measured finite field quota and deferred display service without dispatching guest VSync.
- where: `game/frame/`; `game/boot/graphics_sync.*`
- gap: End-to-end runtime verification waits on RE-02.
- notes: A host frame is never advanced merely to escape guest code. The level start is the guest's own route: retail `0x8007BEC4(level)` at `0x8007AE14`, whose transition loop `0x8007C344` waits on this very field barrier, runs as one resumable guest call spanning 100 display fields with the host presenting each one. Its `[0x800A1174]`, `[0x800A1480]`, `[0x800A155C]` and `[0x800A1370]` writes are therefore the guest's own, not the port's (`game/frame/resident_preparation.*`).

### RE-13 — finite frame ownership
- status: re-partial
- deps: RE-10
- evidence: The retained title owner sequences input, one transition or two resident fields, deferred display work, one finite outer-loop operation, audio and one presentation commit. Native graphics initialization preserves measured state without guest VSync.
- where: `game/frame/`; `game/runtime/toystory2_runtime.*`; `tests/frame_turn_boundary.cpp`
- gap: Reverify the boundary and independent MEMORY/FMV loops through RE-02; issues #25-27 own the incomplete title routes.
- notes: No presentation capability is inferred from prior frames generated by the removed executor.

### RE-25 — actor part drawers and actor incarnation
- status: re-partial
- deps: RE-05
- evidence: From the exact `SLUS_008.93` bytes (`decomp_pipeline.py` plus the disassembly). `0x80025E44` draws each actor of the draw list `0x800A4298` (built by `0x8007863C` from the active list `0x800B18A8` plus the player `0x800B2188`) through `0x8002518C(actor)`, or `0x80024174` when scaled. Parts are 0x6C-byte records at `0x800A9618`, dispatched by part type: 1 → `0x8001CD34` (mode 0) or `0x8001C920`; 2 → `0x80031C3C` via `0x80033C30`; 3 → `0x800396B0`/`0x8003925C`; 5 → `0x8001D240`; 6 → `0x800362C0`; 9 → `0x8001DFE4` (mode 2, Buzz); 10 → `0x80033C58`. A part's faces at `*(part+0x20)` are 0x30-byte quads (type bits `0x3C000000`) and 0x24-byte triangles, ended by a word with no type bits; the drawers hold the pool cursor `0x800A1608` in `s3`, spill `s0..s7` to `0x1F800000`, and store the cursor only on return. `0x8001DFE4` fetches quad corner normals in slot order 0,1,3,2. The actor pool is 64 records of 0x98 bytes at `0x800C2EA0`, slot i bound to placement `0x800B57A8 + i*0x20`; `0x8006A9FC(actor, mode)` resets an actor to its placement, called by the level start `0x8006ADF8` and for respawn from `0x8006CE84` at `0x8006D088`.
- where: `game/render/actor_incarnation.*`, `game/render/actor_producers.*`, `game/render/part_face_drawers.*`
- gap: `0x8001C920` and `0x8001DFE4` are ported and diff-proven (4:3 packets, scratchpad and screens identical to retail-path HEAD). Not ported: `0x8001CD34` (no replay reaches it, so a port cannot be proven), `0x80031C3C`, `0x800396B0`, `0x8003925C`, `0x8001D240`, `0x800362C0`, `0x80033C58`, and the hierarchical drawers `0x8002B6F0`/`0x8002AC40` (compiled C over helpers `0x80084024`, `0x80083F74`, subdivider `0x80084154`). Whether `0x80024174` reaches the same drawers is unchecked.
- notes: A native body charges no guest cycles, so the guest profiler's words (`0x800CF400`, `0x800CF41C..2B`, frame CPU time `0x800A13BC`, written at `0x8003EA2C` with no resident reader) and the exception save slot `0x8000DFF8` differ from a fully guest run; nothing else in RAM does.

## assets

### RE-08 — `.RAW` framing and decompression
- status: re-verified
- deps:
- evidence: A 14-byte chunk header carries both CRCs and the Traveller's Tales flag-bit LZ scheme decodes 813/813 chunks across 46 files to exact lengths with both CRCs intact; LEVEL01 matches an independent extraction. A single chunk can falsely resemble RNC2; the full corpus is the discriminator.
- gap: none for framing and decompression
- where: the guest's own chunk framing and `DecompressRAW` scheme, recorded against the retail corpus in `docs/re-frontier.md` RE-08
- notes: A single chunk can falsely resemble RNC2; the full corpus is the discriminator.

### RE-18 — the level's own object list and token bookkeeping
- status: re-partial
- deps: RE-01
- evidence: All of it read out of a live guest through the control channel (`rw` only — no guest word was ever written) and cross-checked against the decompilations. The area's placed-object table is the word at `0x800A1274`, which points at `[count][entry…]`; `0x800A1274` itself was read as 0x80149F94 with a count of 81 in Andy's House room 1. `FUN_8002EA98` reads `table[slot + 1]`, then `*(entry + 0x10)`, then `*(that + 0x14)` and shifts the three words left by 5 — so those three words are the level's 16-bit object coordinates and the guest's own world units are the same `<< 5`, which is the same shift Buzz's object at `0x800B2188` needs. `FUN_80048638` reads `*(record + 0x18)` and returns `!= 0`: in room 1 exactly 5 of the 54 populated records are non-zero, which is the same count as the token slots. The level's five token ids live at `0x800A8668` as 16-bit values at stride 8 (`0x39`, `0x7968`, `0x3A`, `0x7978`, `0x3B`); `FUN_8007678C` case 2 walks that table with stride 4, compares each object's id against a slot, and on a match stores `2` to `0x800A866C + 16*slot`, `0` to `0x800A8674 + 16*slot`, ORs `1 << slot` into a collected byte, and sets `0x800A1544 = 1` — that flag and those five state words are the guest's own collection oracle, readable while driving.
- where: the control channel's read-only `rw` reader
- gap: The three ids that fall inside room 1's 81-entry table (`0x39`, `0x3A`, `0x3B` → `table[56]`, `table[57]`, `table[58]`) resolve to positions `(8616, 3516, -5469)`, `(16795, 5955, -3919)`, `(19190, 4839, -12319)` in shifted world units, and their second coordinate does not agree with Buzz's own Y (1875), so which field of that triple is height is still unresolved; walking to the XZ of a candidate has not yet raised `0x800A1544`, so the id→object mapping is a hypothesis, not a proven pickup. `FUN_80044B5C`'s classifier is a four-level dereference (`table[i]` → `+0x10` → `+0x10` → `ushort +0x10`) whose result the tool's simpler read does not reproduce: the type word read at `record + 0x10` is never in the ranges its `case 2` can return.
- notes: Two Ghidra renderings of the same table contradict each other (`FUN_80044B5C`/`FUN_80048638` use `DAT_800A1274 + 4` as the array base, `FUN_8002EA98` dereferences `0x800A1274`), and only the dereference form returns RAM pointers; treat the `_DAT_` constants in these routines as unreliable until the instructions behind them are read by hand.

### RE-19 — the FMV overlay's movie selection, entry and return contract
- status: done
- deps: RE-01
- evidence: Decompiled from the exact `FMV/FMV.BIN` bytes (manifest image `toystory2_fmv`, base `0x800D5D20`). `FUN_800D6628`, the module entry the front-end sequencer enters by movie index, switches on its argument and selects a path — index 0 → `toy2fmv\dlogo.str`, 1 → `toy2fmv\tt.str`, 2 → `toy2fmv\acti.str`, and 10..0x1c → `traler2`, `l01in`, `l02in`, `l03bo`, …, `l15bo2`, `end01` — then calls the player `FUN_800d7088(path, startLba, depth, unused, sectorCount, unused, arg)` with depth 3 (the guest's own 24-bit selector, compared against the literal 3 before its `LoadImage` upload), and finally runs four display-rectangle restores plus `FUN_800d88b4`. `FUN_800d7088` zeroes its `arg` when `_DAT_800A1670` (the cold-front-end word this title sets in `finishColdFrontEnd`) is already set, and returns `local_38`: 0 at end of movie, and `_DAT_800A1670` when the guest asks to skip — it sets `local_38` from the pad word at `0x800A1480` bit 3 (Start), gated to after roughly 16 frames. That value is returned unchanged to the sequencer, which treats nonzero as "the cold intro is over", so a skip both ends this movie and skips the rest.
- where: `game/fmv/movie_player.cpp`
- gap: The decompiled per-index `(startLba, sectorCount)` table at `0x800DA2EC` reads as garbage in the file image, so it is initialised at runtime and only its live values are trustworthy; the override therefore takes the movie from the guest's own argument (the path string) and resolves it on the disc rather than re-deriving the table.
- notes: `0x800D5D20` is a shared slot that MEMORY.BIN also occupies, so `0x800D7088` names nothing without the module identity; the native player is installed per published image generation, not per address.

### RE-20 — the level start's transition fades are authored, not a load wait
- status: done
- deps: RE-01
- evidence: Decompiled from the exact `SLUS_008.93` bytes (manifest image `toystory2`, text base `0x80010000`). `FUN_8007BEC4(level)` is straight-line initialisation — it calls `FUN_8003A218`, then `FUN_8007C278`, then the asset-set loader `FUN_8003D88C(param_1)`, then `FUN_8007C344`, then `FUN_8007C5F8` and the rest — with no loop and no VSync of its own; its 56 measured display fields come entirely from the two callees. Both are 28-field fade loops: each sets `iVar2 = 0x1c`, then per field calls the field barrier `FUN_8003FA68(1)`, subtracts the fields elapsed (`DAT_800A1174`), clamps at zero, and steps the fade with `FUN_800775cc`, looping while the counter is nonzero. `FUN_8007C278` opens with `FUN_80077598(0x80,0x80,0x80,0xc)`; `FUN_8007C344` opens with `FUN_80077598(0,0,0,0xc)` and additionally breaks out early on the boot countdown (`_DAT_800C166C`) or on the Start bit in `DAT_800A1480`, which is the level-start cut-short path `ResidentPreparation` reports as `finished`.
- where: `game/frame/resident_preparation.cpp` (`level start` resumable call), `game/boot/graphics_sync.cpp` (`completeOwnedFieldBarrier`)
- gap: closed by measurement: the post-level transition `0x8007BC74(4, 0x40)` is reachable on the pause menu's own EXIT LEVEL route (`replays/toystory2_exit_level_v2.pad`), where the load `FUN_8003A218` + `FUN_8003D88C(0)` presents ZERO display fields and the 240 fields the call returns are `FUN_800DB54C`, the guest's own level-RESULTS screen. See RE-24.
- notes: The level start's 56 fields are the two 28-field fades and are presentation G004 retains; the load between them presents no field and costs no wall-clock stall under instant CD.

### RE-09 — scene, collision and animation formats
- status: todo
- deps: RE-00, RE-08
- evidence: Cross-title Traveller's Tales research suggests a 0x20-byte transform shape but is not evidence for this exact executable.
- where: future checked decoders under `game/render/` or `tools/`
- gap: Confirm every field against exact Toy Story 2 bytes before product use.
- notes: Do not copy unlicensed implementations or assets.

### RE-15 — decoded `.RAW` packet semantics
- status: todo
- deps: RE-08
- evidence: A packet census over the decoded corpus reported 813 chunks, 23,904,134 decoded bytes and 52 distinct header values without assigning meanings.
- gap: Derive Toy Story 2's command table and structures from its loader; another game's numeric IDs are not transferable evidence.
- notes: Unknown IDs must remain explicit rather than silently skipped.

### RE-21 — the level gate is a persisted per-level unlock byte; the game offers no pad-input route past it
- status: done
- deps: RE-20
- evidence: Decompiled from the exact `SLUS_008.93` bytes, and re-derived with `--fresh` to a byte-identical result, so this is not a cached artefact. The outer loop `FUN_8007A9E8` owns the cursor `DAT_800A1530` and resolves each selection through the pointer table `(&DAT_8009DEF8)[DAT_800A1530]` (0x8007AB44 and 0x8007AD28). Completion is a single byte: on finishing a level it reads `byte[0x800C1628 + cursor]`, stores `1` back to the same byte, and, only when the old value was zero, raises the unlock message `FUN_80073408(cursor + 1, 0x1e, 1)`. That array sits inside the front-end state block `0x800C1608` that `FUN_80078C34` prepares and `FUN_80078CC4` commits, so progress is persisted memory, not a session flag. The current level's own completion byte is `DAT_800A1540`, whose only writer in the whole resident image is the level start `FUN_8007BEC4` at 0x8007C208 — it is copied out of the loaded level's data — and whose only reader is 0x8007B13C, where it is compared against `byte[table[cursor] + 0x800C1617]` to decide whether the "level finished" branch is owed. The level-select screen skips a candidate when that per-level byte is nonzero while `DAT_800A1540 == 0`. The game's own route to later levels without completing anything is ATTRACT/DEMO mode, and it is reachable with NO input at all: the outer loop polls the front end with `FUN_8007BC74(2,0)`, and when that poll returns event 0 it stores `1` to `DAT_800A120C` and falls straight into `LAB_8007ADF0`, which resolves the level from the cursor and calls the level start. With `DAT_800A120C == 1` the loop rotates `DAT_800A14D8` through levels `0, 3, 7, 10, 0xd` (case 5). Idle at the title, this loads an arcade interior and later a Wild West barn yard carrying the guest's own `DEMO MODE` text, then returns to the title. Level index 1 is NOT in that rotation, so it does not reach the second level, but it IS a pad-only route into several later areas.
- where: `game/frame/frame_driver.cpp` (`kPlaybackLevel = 0x800A1530`, `selectPlaybackLevel`), `game/frame/outer_loop.cpp`
- gap: None for the gate itself. It cannot be opened by pad input, so no replay reaches level index 1; `0x8007BC74(4, 0x40)` stays unmeasured by that route.
- notes: The level-select screen's own skip test was read from the same bytes in manifest image `toystory2_memory` (`BITS/MEMORY.BIN`): its draw routine advances a scroll position and, when `DAT_800A1540 == 0` and `byte[table[cursor] + 0x800C1617]` is nonzero, increments past the candidate — the same lock as the resident side, seen from the screen that applies it. A cheat sequence was searched for and NOT found on evidence: the Konami-style mask constants (`0xFEDC`, `0xBA98`, `0xBBAA`, `0x1111`, `0x4444`) appear nowhere in the resident text or in `BITS/MEMORY.BIN`, and the only resident function that reads the pad word `DAT_800A1480` more than once, `FUN_800742C4`, is the PAUSE MENU. Playing a level to completion was abandoned for the same reason (issue 0039). Consequently level index 1 is unreachable from any verified route, and every later area depends on it.
- toolchain note: this disc's instruction words are stored LITTLE-endian while its data is big-endian, which `external/psxport/tools/decomp/images.py` documents at its 32-bit instruction reader. A hand-rolled BIG-endian decoder of this image decodes to plausible-looking nonsense and makes correct module geometry look broken; `code_first` 0x8F4 for `toystory2_memory` really is `0x27BDFFE8` read little-endian, and the same holds for `toystory2_fmv` at 0x908.

### RE-22 — the attract route's "loading card" IS the level start's first fade, forced by demo mode
- status: done
- deps: RE-20, RE-21
- evidence: Static, from the exact `SLUS_008.93` bytes. `gfx\loading.raw` is the card's art: `GFX/LOADING.RAW`, 10,932 B, RNC2-compressed. Its name sits at 0x80021E94 in MAIN's asset-name table whose head is 0x80021E80 (`bin`, index 0; `gfx\loading.raw` index 2). `--refs 0x80021E94` returns exactly one reference, `addiu a0,a0,0x1e94` at 0x8003FB5C inside `FUN_8003FB0C` — the scene-graphic selector, which switches on an id and loads the chosen name through the compressed loader `FUN_8003B544`. Its `case 0` is `gfx\loading.raw`, and `case 0xffffffff` is `gfx\film.raw`. `FUN_8003FB0C` has exactly two callers: 0x80041338 (`FUN_800412F0`) and 0x8007C2A8 (`FUN_8007C278`). `FUN_8007C278(param_1, param_2)` opens `if ((param_2 != 0) && (param_2 != 0x7b)) { param_1 = 0; }` and is called from the level start `FUN_8007BEC4` as `FUN_8007C278(DAT_800A16A8, DAT_800A120C)`. `DAT_800A120C` is the attract/demo flag RE-21 identified. So on the attract route demo mode is set, the guard forces id 0, `gfx\loading.raw` is loaded, and the routine's 28-field loop (`FUN_8003FA68(1)` barrier, `FUN_800775CC` fade step, `FUN_80046A88` draw) presents it.
- where: `game/frame/resident_preparation.cpp` (level start), `game/frame/frame_driver.cpp`
- gap: none on the attract route. `LevelStartPresentation::arm` (`game/boot/level_start_presentation.cpp`) installs the `0x8007C278` override only when this guard fires, from the `DAT_800A120C` the level start publishes, and retires it otherwise, so a PLAYER level is not intercepted at all and its 28 fields are `FUN_8007C344`'s authored transition fade alone.
- notes: THIS CORRECTS RE-20 for this route. The level start's first 28 fields are not a pure authored fade: under attract/demo they are a loading-only screen, which is a G004 violation, and its owner is `FUN_8007C278` rather than the overlay screen dispatcher (RE-21's probe of `FUN_800D95C4` is consistent — ids 10 and 2 only, because this path never dispatches a screen). The guard's two exceptions, `param_2 == 0` and `param_2 == 0x7b`, are the routes that legitimately show this graphic.

### RE-23 — the front-end menu gates the player's Start press twice before attract can take over
- status: done
- deps: RE-21
- evidence: `BITS/MEMORY.BIN`, manifest image `toystory2_memory`. The attract entry is `FUN_8007A9E8` storing 1 to `DAT_800A120C` when the front-end poll returns 0; that poll is `FUN_8007BC74(2,0)` -> `func_0x800d95c4(2)` -> `_DAT_800A1374 = FUN_800d92c4()`, and the handler returns `DAT_800E543C`. The handler reads the pad word `_DAT_800A1480` and edge-detects it against the previous frame's `_DAT_800A11E4`: Start is bit `0x8`, tested as `if (((_DAT_800a1480 & 8) == 0) || ((_DAT_800a11e4 & 8) != 0))` skip, i.e. a NEW press only. A second test uses bit `0x1`. Two gates then stand between that press and the menu acting. `uVar3` is zeroed once per handler pass and counts up, and the Start branch requires `0x1e < (int)uVar3` — a 30-field input lockout from entering the handler. Separately `bVar1 = _DAT_800C1668 != 1`, the Start branch requires `bVar1`, and the same flag selects the idle threshold `iVar4` of 900 fields normally versus 300 when `_DAT_800C1668 == 1`.
- measurement, RESOLVED: the pad word is NOT lost. On menu frames (the intro movies never service the front end's pad word, so any reading taken there is meaningless), with Start held over pad frames 820-823, `_DAT_800A11E4` reads `0x0008` in the dump at 825 and `_DAT_800A1480` is the current word, and a per-turn probe of the same two words plus `DAT_800E543C` shows the sequence `0x8/prev 0` -> `DAT_800E543C = 2` -> the 0x17-field fade countdown -> the main menu, all from ONE Start press, and `DAT_800A120C` stayed 0 throughout. The input path is intact; the replay was simply pressing at absolute pad frames that fall inside the lockout or inside a movie. THE LOCKOUT IS 30 DISPLAY FIELDS, not 30 handler passes: `uVar3` accumulates `_DAT_800A1174`, the elapsed-field word the field barrier publishes, so what decides a press is how many fields the title screen has been up, not how many times the handler has run. Replayed with one Start after the lockout, the front-end poll returns event 1 with `DAT_800A120C == 0` — the PLAYER leg, never attract. The main menu is the SAME guest call (`FUN_800D92C4` falls into `FUN_800D7D78`), and its confirm word is `0x4000`; the level-select screen answers `0x4000` as well. A player route is therefore: Start after the title's lockout, Cross on START GAME, Cross on the level. The recording `replays/toystory2_player_andys_house_v2.pad` (phase-keyed, five segments) does exactly that and reaches the level with `DAT_800A120C == 0` and `DAT_800A16A8 == 1`; `..._v2.pad` is the same route plus the transition Cross that carries it into the play loop.
- where: `game/input/recording_phase.*` (`InputPhase`), `game/boot/level_start_presentation.*`, `game/frame/resident_preparation.cpp`, `tools/headless_run.py --pad`
- gap: none for the gate itself: the post-card stall was never a CD wait, so nothing was changed. `DAT_8009FDF0 == 5` is boot residue (`FUN_8008B6DC`, reachable only from the one-time init `FUN_8003A780`), and the resume PC `0x8007C3E0` is +156 bytes inside `FUN_8007C344` — the level start's own transition fade, whose `do/while (iVar2 != 0)` decrements `iVar2` ONLY under `bVar1`, which a player level starts without because `FUN_8007BEC4` passes `DAT_800A120C == 0` as `param_2`. Its only exit is the rising Cross edge (`DAT_800A1480 & 0x4000`, previous word clear) that arms `bVar1` and starts the 28-field countdown; the Select branch needs `-1 < _DAT_800C166C`, and that word is `-30` forever (three writers in the entire executable, all in `FUN_8007A9E8`'s boot init, none incrementing it). The attract route never stalled because `DAT_800A120C != 0` there, so `bVar1` starts true — the asymmetry that made this look route-specific. The port had delivered every press it was given, so `replays/toystory2_player_andys_house_v2.pad` adds the Cross and returns the level start in 177 display fields, with And's House in the resident play loop.
- notes: This reframes "every pad route ends in demo" from a property of the game into a defect with two named candidate causes, and the level-select is NOT simply unusable: a press inside the lockout is discarded by the guest, not lost by the port.

### RE-24 — the memory dispatcher's two argument roles, the post-level transition's results screen, and why attract never reaches it
- status: re-verified
- deps: RE-20, RE-21, RE-22
- evidence: Decompiled whole from the exact SLUS_008.93 bytes (resident exe scratch/bin/toystory2/SLUS_008.93, MEMORY.BIN scratch/flat/BITS__MEMORY.BIN), every target reported body present. (1) FUN_8007BC74(param_1, param_2) is 28 instructions: save DAT_800A16A8, zero it, and if param_2 != -1 store DAT_800A138C = param_2 + 0x43E, call FUN_8003A218() then FUN_8003D88C(0), then call func_0x800d95c4(param_1), then restore DAT_800A16A8. So the FIRST argument only selects a screen and the SECOND only sizes a fade; the asset set is ALWAYS FUN_8003D88C(0). (2) FUN_800D95C4 in MEMORY.BIN switches param_1 over 1,2,4,5,6,8,9,10,0xb, and FUN_8007A9E8 — the main loop, decompiled whole — calls the dispatcher with exactly (10,0),(2,0),(5,0x40),(0xb,0xc0),(4,0x40),(9,0x80),(8,0) and nothing else, so arms 1 and 6 are dead in this executable. Arm 6 is the binary's OTHER loading-only screen: FUN_800D7B2C(0, 600, 600), a 600-field fade from black over graphic id 0 = gfx\loading.raw; arm 1 is FUN_800D7B2C(1, 0xa0, 0xa0). (3) Arm 4 is FUN_800DB54C: the level-RESULTS screen — five toy slots from _DAT_800A1540 and byte[0x800C1617 + level], a second row from the authored level, the coin count, the medal row, PRESS-X-TO-EXIT — entered after func_0x8003d084(0), then one FUN_8003FA68(1) per field. Its counter iVar14 starts at 0x168 (360) and the arm 0x3b < iVar14 - 0x79U PARKS it at 180, so the screen leaves only on a real rising Cross (DAT_800A1480 & 0x4000 with the previous word clear) while iVar14 < 0xf0, which then re-arms 120 fields while the token or coin counters are nonzero and 24 fields otherwise. The self-shortening path that manufactures that edge (store 0x4000 into DAT_800A1480 and clear it from DAT_800A11E4 every field after a 900-field countdown) sits inside if (-1 < _DAT_800c166c), which is FALSE here: _DAT_800C166C is -30 and frozen — three writers in the resident executable, all in FUN_8007A9E8's boot init, none incrementing it, and ZERO references to the word from MEMORY.BIN — so the results screen waits for the player's Cross, exactly as its PRESS-X-TO-EXIT caption says. (4) The attract rotation in FUN_8007A9E8 switches DAT_800A14D8 0..4 to cursor 0,3,7,10,0xd, exactly the table CoreFrameBoundary::selectPlaybackLevel mirrors. (5) The level table at 0x8009DEF8 read from the exact bytes is cursor -> level number 1,2,6,4,5,3,7,8,9,10,11,12,13,14,15,0xffffffff; THIS disc's DATA words are little-endian as well as its instruction words, so a big-endian read gives 0x01000000, 0x02000000, ... and the table looks like a pointer table. (6) FUN_8003D88C's name table at 0x80021D18 is the 15 asset-set names level01\level01 .. level05\level15.
- where: game/frame/frame_boundary.cpp (selectPlaybackLevel, pollLevelTransitionEvent, callMemoryDispatcher)
- gap: None for the split. Still unmeasured: the loads inside a SECOND level (every attract leg but Andy's House loads a level no player route reaches) and the MEMORY dialog's own load 0x8007BC74(9,0x80) -> FUN_800D6A74, whose per-field countdown is driven by DAT_800E5468 and has never been reached.
- notes: This is why the attract route CANNOT inventory the post-level transition, and it is a property of the executable, not of the port: the demo's level exit sets DAT_800A136E = 3, which branches to LAB_8007AAEC (the cold front-end leg, 0x8007BC74(10,0) plus the four intro films) and never to the DAT_800A136E == 1 leg where 0x8007BC74(4,0x40) lives. Measured over a 19,000-frame attract run covering all five rotation legs, the guest call 'level transition' appears 0 times. The one route that reaches it is the pause menu's own EXIT LEVEL, which is a leave-now, not a completion.
