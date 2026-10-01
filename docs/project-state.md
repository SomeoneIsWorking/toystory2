# Project state

Epic intent lives in `docs/project-goals.md`, ownership in `docs/codemap.md`, open bugs in
`docs/issues/`.

**Comparison baseline**: the unmodified USA PlayStation release `SLUS_008.93` in a general-purpose
emulator. Intended deltas: a standalone native/dynarec product, host-owned finite frame iteration,
native input and render ownership, true widescreen, interpolated 60 fps.

| ID | Capability | State | Evidence or exact gap |
|---|---|---|---|
| S001 | Retail executable, disc files and loaded modules reproducibly identified and placed | verified | `tools/extract_exe.py` matches 598,016 B against `docs/info/exe-identity.txt`; `tools/overlay_map.py` derives LEVEL `0x800D12C0`, shared MEMORY/FMV `0x800D5D20`, FMV entry `0x800D6628` |
| S002 | psxport executes resident and streamed code through a gameplay dynarec with classified bounded fallback | partial | Title, menu, level select, LEVEL01 and Andy's Room run through Lightrec; no clean whole-run exit with the fallback ledger (issues 25/26/27) |
| S003 | Native owners provide finite frame, timing, input, audio and presentation sequencing | partial | `game/loop/` owns main's pre/resident/post routes and resident preparation; MEMORY and FMV loop owners still guest-owned (issues 25, 26, 27) |
| S004 | The product boots through front end and LEVEL01 gameplay | verified | Headless route start@500, cross@560/620/790 reaches Andy's Room; `tools/verify_route.py --route` judges from guest RAM at pad frame 900 |
| S005 | Host input produces repeatable guest gameplay behavior | partial | `tools/verify_movement.py`: 40 Up frames move Buzz's guest position words, no-input run rejected; pause/camera/other buttons unowned (issue 20) |
| S006 | Guest-rendered 15-bit screens and gameplay present coherently | partial | Title, menu, level select and Andy's Room present non-black at 960x720; no HUD seen, audio and sustained presentation unchecked |
| S007 | Toy Story 2's 24-bit MDEC movies present coherently | partial | Four intro movies play to their returns; 24-bit capture is visibly corrupted (issue 22) |
| S008 | Authored projection is published to title-owned consumers | partial | Projection leaves `0x80083CD4`/`0x80083CF4` are typed in `guest_facts.h` and asserted at the C++ boundary; live reach unproven |
| S009 | Visible scene layers have native game-state producers | missing | Camera/visibility/mesh inputs are captured; no title-owned world, actor, effect or 2D producer emits the picture (issue 30) |
| S010 | True widescreen composes correct title and gameplay pictures | missing | Resident canvas, projection centre and visibility window widened (`game/render/resident_widescreen.*`); expanded canvas crosses fixed VRAM parity without a semantic producer (issue 30) |
| S011 | Presentation interpolates stable authored state at 60 fps | missing | Camera history exists; no identity-matched object history or visible consumer, and blocked on S009 |
| S012 | Traveller's Tales `.RAW` assets are reproducibly framed and decompressed | verified | `tools/raw_probe.py` framing plus `tools/raw_unpack.py` both CRCs over the retail corpus |
| S013 | The fresh-clone launcher builds and starts the intended product | partial | `run.sh` → `bootstrap.py` → `tools/run.py` builds `build/player/bin/toystory2_port` and launches only that target; cold real build/launch not rerun since the runtime migration |
| S014 | Hosted CI qualifies each shipping platform | partial | `.github/workflows/ci.yml` runs the asset-free product gate on Linux; no Windows, macOS or Android job (no package boundary exists yet) |
| S015 | Loading completes without loading-only waits; logos cancel through the recovered route | missing | Nothing removed: no native override of the blocking whole-file read `0x80082608`, no bound on the two unbounded loader loops, no loading census kept in this tree |