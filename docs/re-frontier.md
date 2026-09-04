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
- evidence: C009 and I008. `tools/verify_crt0.py` walks 43 retail instructions and derives BSS `[0x800A1070,0x800D12C0)`, stack `0x80200000`, heap base `0x800D12C0`, heap size `0x126D40`, gp `0x800A0CD8`, libc init `0x80089344`, game main `0x8007A9E8`, and entry `0x80082D60`.
- where: `tools/verify_crt0.py`; `game/core/game_config.cpp`
- gap: none for the exact boot group
- notes: The verifier consumes psxport's neutral PS-X EXE parser, not an execution generator.

### RE-02 — dynarec-only execution boundary
- status: in-progress
- deps: RE-01, RE-03
- evidence: The title build and source route guest calls and image-scoped overrides through `game/core/guest_execution.*`; `tools/check_structure.py` rejects every retired static product marker and generated artifact.
- where: `game/core/guest_execution.*`; `external/psxport/runtime/cpu/`
- gap: Issue #31. psxport intentionally reports that its Lightrec dynarec-only backend is not linked. The maintained fork must eliminate interpreter paths before execution can advance.
- notes: Do not restore an offline translator, generated corpus, static dispatcher, engine selector, or interpreter fallback.

### RE-11 — FMV path parser and shared BIOS toupper
- status: re-verified
- deps: RE-03
- evidence: C016. Retail FMV call `0x800D8E48` reaches executable Sony leaf `0x80082E5C`, selects BIOS A0:0x25, and stores the normalized byte at return `0x800D8E50`.
- where: `tools/verify_fmv_boundary.py`; shared implementation belongs in psxport
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
- gap: Reproduce gameplay entry, pause/unpause, held-camera movement, streamed-module replacement, audio and sustained progression through Lightrec with no interpreter symbols.
- notes: Instrument reach and input delivery denominators before interpreting absence of a symptom.

## overlays

### RE-03 — overlay load bases and runtime identities
- status: re-verified
- deps: RE-00
- evidence: C010, C014 and I009. LEVEL alternatives load at `0x800D12C0`; MEMORY and FMV reuse `0x800D5D20`; FMV entry `0x800D6628` is file+`0x908` and begins prologue word `0x27BDFF10`. The corpus contains 22 code modules.
- where: `tools/overlay_map.py`; `game/core/game_config.cpp`
- gap: none for the proven module set and two reused physical slots
- notes: Runtime identity is authenticated image, generation, and address; address alone is ambiguous.

## cd

### RE-04 — CD load chokepoints
- status: re-partial
- deps: RE-01
- evidence: C010 and C012 locate game loader `0x80082508(path,dest)`, inner `0x80082608`, CdSearchFile `0x80092AE8`, CdRead `0x80093AF0`, CdReadSync `0x80093BF4`, command owner `0x80091DE4`, sync loop `0x80091898`, and result service `0x80091310`.
- where: `tools/verify_cd_command.py`; `game/cd/stock_libcd_layout.h`
- gap: The title's `(path,dest)` loader does not fit the legacy `(dest,lba,size)` seam; typed runtime ownership remains incomplete.
- notes: Do not special-case a BIOS call or fabricate load completion.

## frame and input

### RE-05 — authored render-source boundary
- status: re-partial
- deps: RE-01
- evidence: Resident buffers are `0x801BBD28` and `0x801DD21C`; packet pools begin at `0x801BBFEC` and `0x801DD4E0`. Camera producer `0x8002C848`, scene root `0x8002A070`, visibility lists `0x800BB4D8`/`0x800C0AB0`, owner `0x8002622C`, and mesh submitter `0x800100E4` are grounded from binary and earlier reached observations.
- where: `game/render/resident_camera_history.*`; `game/render/resident_scene_history.*`; `game/render/resident_mesh_format.*`
- gap: Material/texture semantics, 2D submitters, and visible native producers remain missing under issue #30.
- notes: OT or GP0 replay is post-GTE and cannot provide true widescreen or interpolation.

### RE-06 — pad driver buffers
- status: re-verified
- deps: RE-01
- evidence: I018 derives buffers `0x800CF8A0`/`0x800CF8C8`, driver pointers `0x800A3E98`/`0x800A3F88`, `0xF0` stride, and consumer `0x8003AC58`. The verifier exercises active-low Cross and release.
- where: `tools/verify_pad_buffers.py`; `game/input/native_pad_owner.*`
- gap: End-to-end response through the current dynarec belongs to RE-17.
- notes: Native input writes the measured packet; it does not emulate an unrelated SIO protocol locally.

### RE-07 — projection publication
- status: re-partial
- deps: RE-01
- evidence: C023 and I019 derive SetGeomOffset `0x80083CD4`, SetGeomScreen `0x80083CF4`, and authored initialization `256/120/160`. The hermetic title boundary checks guest and host effects.
- where: `tools/verify_projection_publication.py`; `tests/toystory2_projection_boundary.cpp`; `game/render/guest_widescreen.*`
- gap: Remaining projection/culling writers and current live reach are unverified.
- notes: Publication does not itself implement widescreen.

### RE-10 — title field timing ownership
- status: re-verified
- deps: RE-01
- evidence: Retail VBlank callback `0x80039D60` advances the state consumed by wait `0x8003FA68`. The native frame owner instead supplies the measured finite field quota and deferred display service without dispatching guest VSync.
- where: `game/loop/`; `game/boot/native_sync_overrides.*`
- gap: End-to-end runtime verification waits on RE-02.
- notes: A host frame is never advanced merely to escape guest code.

### RE-13 — finite frame ownership
- status: re-partial
- deps: RE-10
- evidence: The retained title owner sequences input, one transition or two resident fields, deferred display work, one finite outer-loop operation, audio and one presentation commit. Native graphics initialization preserves measured state without guest VSync.
- where: `game/loop/`; `game/core/toystory2_runtime.*`; `tests/toystory2_frame_driver_boundary.cpp`
- gap: Reverify the boundary and independent MEMORY/FMV loops through RE-02; issues #25-27 own the incomplete title routes.
- notes: No presentation capability is inferred from prior frames generated by the removed executor.

## assets

### RE-08 — `.RAW` framing and decompression
- status: re-verified
- deps:
- evidence: C019 and I015. Traveller's Tales' flag-bit LZ scheme decodes 813/813 chunks across 46 files to exact lengths with both CRCs; LEVEL01 matches an independent extraction.
- where: `tools/raw_probe.py`; `tools/raw_unpack.py`
- gap: none for framing and decompression
- notes: A single chunk can falsely resemble RNC2; the full corpus is the discriminator.

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
- evidence: `tools/raw_packet_census.py` reports 813 chunks, 23,904,134 decoded bytes and 52 distinct header values without assigning meanings.
- where: `tools/raw_packet_census.py`
- gap: Derive Toy Story 2's command table and structures from its loader; another game's numeric IDs are not transferable evidence.
- notes: Unknown IDs must remain explicit rather than silently skipped.
