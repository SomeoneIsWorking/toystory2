# Codemap — what's where, what's done, what's missing

The orientation map: consult it at the START of a task to avoid re-deriving structure, and update it in
the SAME commit that lands or changes a subsystem. A stale map is worse than none — a subsystem is marked
done only when VERIFIED on real data, never to look better.

Companions: `docs/re-frontier.md` (the ordered RE steps: real vs hack), `docs/references.md` (the external
RE and format work, with licences, and the fact that no decomp exists), `docs/info/` (claims +
instruments), `docs/issues/` (what has been tried and ruled out).

**Status vocabulary:** ✅ verified on real data · 🟡 partial (gap named) · 🔬 in progress · ⬜ not started ·
❓ unresolved question · ➖ not applicable to this game · 🔴 regressed.

## Read this before anything else: the honest state, 2026-08-24

**The real generated port now renders a stable retail title headlessly.** RE-00
supplies Ghidra, RE-01 verifies crt0, and RE-03 verifies both resident overlay slots. RE-02 emits the
executable and all 22 proven code modules, links `toystory2_port`, and matches the independent CPU oracle 34/34 at the first call.
The first empirical resume seed (`0x80088A2C`) is classified and wired; live boot now opens the disc,
services the retail stock-libcd command/result path and performs sector DMA. RE-04's first missing
state was generic CDC following-sector drive pacing, not an unreturned `0x80091DE4` command. Psxport
`3418a79b` first removed that race; the last live route on `d2266f4b` advances the guest through
eleven ReadN phases, while current pin `bc8c8897` retains the path under the full compile/static gate.
RE-10 then
closes the first render blocker: graphics init registers VBlank `0x80039D60`, but no host field turn
ever invoked it, so `0x8003FA68` waited forever. The game-local field clock now delivers that exact
callback at the GPU rate. Non-black presents show the retail legal text and ESRB card; boot then enters
emitted FMV at `0x800D6628`, completes the shared BIOS `A0:0x25` path parser and traverses the exact
32-way resident renderer table. RE-13 then closes the missing guest-loop frame fence through the
neutral shared `FramePresenter::commit`: the stable title renders. RE-14 classified and seeded the
LEVEL01 entry `0x800D12C4`; the route now advances to 8,320 commits and terminates at unmapped read16
@ 0xEDA4F893 (RE-16). C021 classifies it as a guest model-pointer consumer rather
than hardware; writer provenance remains open.

**AND THERE IS NO DECOMP OF THIS GAME.** No symbol map, no function boundaries, no matching build for
`SLUS_008.93`; the sibling ports each vendor one and this tree vendors nothing but the framework. Every
address this port ever holds must come from this executable's binary evidence. RE-00, RE-01, RE-03,
RE-08, RE-10 through RE-14 are complete; RE-02 and RE-04 are partial, with the post-seed unmapped-read
boundary (RE-16) the live frontier.
`docs/references.md` records how the no-decomp
negative was established and that a search negative is weaker than a measurement.

**The defining structural fact, measured:** **Toy Story 2 STREAMS CODE OVERLAYS.** **21 plain overlay files** hold
245,148 bytes = **29.1% of the game's code-bearing bytes** — `LEVEL01..LEVEL10/{LEVEL,LEVEL1}.BIN` (20)
plus `BITS\\MEMORY.BIN`. (`LEVEL00\\LEVEL.BIN` carries an overlay name but is a 4-byte placeholder holding
no code — not scannable, not counted.) Of 274 disc files scanned in full (24.5 MiB), exactly **23 contain
a `jr $ra`**: those 21, the boot executable, and `FMV\\FMV.BIN` (68 of them). Exact load/call evidence
now proves FMV is a 22nd entered code module at the shared `0x800D5D20` slot; its mixed 510,960-byte
file is kept separate from the old plain-overlay byte percentage. Every file other than those 23 has zero. The modules read
74.6–98.8% code-plausible against the boot exe control's 92.4%, and **67.9–83.4% of the 16 `LEVEL*.BIN`
modules' `j`/`jal` targets land inside the boot exe's `.text`** (the boot exe's own figure is 91.5%), so
they are statically linked against the engine and call into it; the 4 stub `LEVEL1.BIN` files have zero
`j`/`jal` (no evidence) and `BITS\\MEMORY.BIN` reads 50.6% — its own figure, below the LEVEL band but far
above the `PATH*.BIN` trap case's 0%.
The trap case duly appeared and was rejected: `PAD/PATH*.BIN` reads 48.9–87.2% code-plausible with ZERO
`jr $ra` and ZERO `j`/`jal` — waypoint coordinates whose bytes decode as legal instructions. Full
denominators: `docs/info/claims/002-*`.

This is the Vagrant Story answer, not the Mega Man X4 answer. **Consequences:** `GameConfig` carries
multiple overlay load bases and the RAM map is NOT static. RE-03/C010 closes that dependency: the loader
selects exactly one of `LEVEL{,1,2,3}.BIN` and loads it at `0x800D12C0`; `BITS\\MEMORY.BIN` is co-resident
at `0x800D5D20`. The 19,040-byte interval is exactly the largest level module and five LEVEL+LEVEL1
pairs exceed it, proving alternative contents of one slot. The old `0x800D1000` result remains only the
4 KiB-granular fit from C003.

**BLIND SPOTS of everything above, stated because the method cannot see past them:** the census detects
PLAIN R3000A code, so it asserts *"TS2 streams plain absolutely-linked overlays and nothing else on the
disc is plain code"*, NOT *"there is no other code on the disc"* — compressed or packed code would read as
a texture. 26 files / 525 MB of `TOY2FMV/` (95.3% of the disc) was never scanned; that is the largest
unscanned mass and "unmeasured" is not "absent". Fragments under 64 bytes are invisible by construction.

## The two halves

| | |
|---|---|
| `external/psxport/` | the PSX-generic framework: a symlink to the workspace's one shared checkout, or a private clone at `psxport.pin` on a standalone machine. It owns the MIPS→C recompiler, runtime substrate, hardware backends, SDK HLE and harness. Framework changes land in the framework repo, not here. |
| `game/`, `tools/`, `generated/` | this port: the seam, the RE, the provisioning, and (eventually) the recompiled substrate. |

There is no third game layer and **there is no decomp to vendor**. The one library worth considering
later candidate `temisu/ancient` was measured irrelevant to TS2 `.RAW`: the payload is TT's own LZ
codec, now implemented and corpus-verified in `tools/raw_unpack.py`. `docs/references.md` records the
falsified RNC branch and licensing evidence.

## Subsystems

| subsystem | where | status | notes |
|---|---|---|---|
| Disc resolution | `tools/resolve_disc.py` | ✅ | One implementation of CLI arg > `$PSXPORT_TS2_DISC` > `.env` > a `*.chd` in the repo root; refuses (exit 2) naming all four sources rather than returning empty, and refuses when a configured path does not exist instead of falling through to a different disc. `tools/run.py`, `extract_exe.py`, `extract_disc_files.py` and `discdump.py` all go through it. |
| Disc listing | `tools/discdump.py` | ✅ | Thin wrapper over the framework's own `discdump`, resolved through `$PSXPORT_DIR` and built into this repo's gitignored `scratch/build/discdump-<checkout hash>/` — never into the shared framework checkout. Refuses (exit 2) when `discdump` is missing, the disc is unresolvable, or a read yields zero entries. Verified here: 300 entries read from the retail USA CHD — and 300 is the denominator every doc in this tree must quote for a disc-wide negative (`grep -ci NGN` → 0 of 300). |
| Disc → executable provisioning | `tools/extract_exe.py` | ✅ | Extracts `SLUS_008.93` (598,016 B), prints the PS-EXE header, and checks SHA-1 + size against `docs/info/exe-identity.txt`. Real output: MATCH `f90c9cd6…`. **Weaker than the sibling ports' equivalent and it says so on every run:** the expectation is OUR OWN measurement, because no decomp declares a target hash for this executable. Refuses (exit 2) rather than passing quietly if the identity file is missing. |
| Ghidra RE supply | `tools/ram_image.py`, `tools/ghidra_xref.py`, `tools/re_xref.py`, framework `external/psxport/tools/decomp.sh` | ✅ | RE-00 / C008 / I005. A fresh Ghidra 12 `MIPS:LE:32:default` import over the verified image reported analysis success; the xref gate passed 10/10 fold controls plus 5/5 independent Ghidra/fold controls; the framework decompiler emitted the header entry at `0x80082D60`. Issue #5 records and fixes the prior PyGhidra mode-detection failure. Derived images/projects/C stay under gitignored `scratch/`. This verifies the supply, not Ghidra's guessed types or any game subsystem. |
| Flat scan corpus | `tools/extract_disc_files.py` | ✅ | 300 listed → 26 TOY2FMV skipped → 274 extracted **and size-verified** → 0 failed (24.5 MiB in `scratch/flat/`). Refuses (exit 2) on zero extracted; prints the TOY2FMV denominator gap every run. |
| Code / overlay census | `tools/code_scan.py` | ✅ | The three-signal detector behind the overlay finding — a verbatim copy of the sibling Mega Man X4 `code_scan.py` (identical logic and thresholds), which imports the calibrated code/data discriminator from `external/psxport/tools/exe_similarity.py` so there is ONE calibrated copy. `--selftest` run here on TS2's own data: POSITIVE 99.2% at pc0, NEGATIVE 0.1% on a VAB sample bank, PASS. `--census` refuses on a zero-match glob. The same unmodified code gave Mega Man X4 the OPPOSITE structural answer, which is the best evidence it measures files rather than expectations. |
| Overlay load-base fit | `tools/base_fit.py` | ✅ | The instrument behind `claims/003`. `--selftest` run here: 10 of 10 `LEVEL*\\LEVEL.BIN` fit a base, **exactly** 9 at `0x800D1000` with `LEVEL09` at `0x800D2000`; 150 asset files and the 5 `PATH*.BIN` trap files all rejected; PASS. A **DERIVATION gate** (added 2026-08-12 after review) is what makes the reported base evidence rather than an echo: the `LEVEL09` disagreement, a re-score at the reported base that must equal the claimed hit count, and a decoy base one page up that must score strictly worse. Shown catching the defect it was written for — splicing `h1, b1 = scores[0][0], 0x800D1000` into `fit()` (report the recorded slot for every file, keep the true argmax's hit count) previously printed PASS and now exits 1 with 4 FAILs. Refuses (exit 2) on a missing corpus AND on a missing boot exe (without a `.text` range every file would appear to fit something). Belongs in `external/psxport/tools/port/` under the tooling-hoist decision; lives here because a game repo may not edit the framework. |
| Exact overlay map | `tools/overlay_map.py` | ✅ | RE-03 / C010 / C014 / I009. Derives LEVEL `0x800D12C0`, MEMORY/FMV `0x800D5D20`, and exact MEMORY frontier. It also exact-checks FMV load `0x8003EEAC`, immediate entry call `0x8003EEC4 -> 0x800D6628`, and the retail file+`0x908` prologue. Selftest passes 10/10 including the forced opposite slot answer. |
| `.RAW` container probe | `tools/raw_probe.py` | ✅ | The instrument behind `claims/005`. `--selftest` run here: 39/39 chunk CRCs on `LEVEL01\\LEVEL.RAW` to a sentinel; a `.DAT` (exit 2), a truncation (exit 1) and a one-byte payload flip (39 chunks, `crc_ok=38 crc_bad=1`, exit 1) all rejected; PASS. That third negative is what proves the CRC check fires. **All four classes are now asserted on `report()`'s own EXIT STATUS, not on `probe()`'s internal tuple** (fixed 2026-08-12 after review): `report()` returned 0 whenever `crc_bad == 0`, so a truncated `.RAW` — a valid prefix that never reaches the sentinel, 3,450 bytes unexplained — exited 0 in the machine-readable channel while the selftest's tuple assertion passed. A walk that stops anywhere but the sentinel is now exit 1, and reverting that check makes NEGATIVE B fail. Framing only — it does not decompress. Its `walk()` is the ONE chunk-walk implementation in this tree; `tools/raw_unpack.py` consumes it. |
| `.RAW` decompressor | `tools/raw_unpack.py` | ✅ | RE-08 / C019 / I015. Faithful transcription of TT's own `DecompressRAW` LZ scheme (mateusfavarin/tsr, MIT — game-code-derived), replacing the RNC hypothesis: RNC new+old × method 1+2 all MEASURED failing (38–39/39 LEVEL01 chunks) before the TT decoder passed. Corpus verdict **46 files / 813 chunks, both CRCs verified each**, exit 0 (`scratch/logs/raw_unpack_corpus.txt`); decoded LEVEL01 output byte-matches all 39 mouksx independent extractions. Hermetic `--selftest` gates five classes through the production seam: positive literal dual-CRC decode; non-.RAW refused (2); missing exact sentinel (1); flipped payload byte via packed CRC (1); flipped unpacked-CRC field (1). Issue #18 records why exact sentinel identity matters. TRAP: LEVEL01#19 alone also decodes under RNC2-new — only the corpus discriminates. Next layer: per-chunk LE u32 command IDs = RE-15. |
| Registries | `tools/re_frontier.py` (shim), `tools/info.py`, `tools/catalog.py` | ✅ | The RE-frontier tracker is a SHIM onto the shared engine at `external/psxport/tools/port/re_frontier.py` — this repo grows no fork of it. `check` parses 17 entries with 0 hacks. C021/issue #17 classify RE-16 as a model-pointer consumer with writer provenance still open. `info.py` and `catalog.py` are COPIES (no hoisted engine exists for them yet) — see the known costs below. |
| Publication audit | `tools/go_public.py` | 🟡 | Copied from the sibling Mega Man X4 tooling (the already-fixed copy: it used to print "clean ✓ — ready to publish" over zero blobs scanned). CONFIG retuned for this game: `*.chd`/`*.cue`/`*.ecm`/`*.pbp` and `SLUS_008.93`/`SLUS_00893` as copyright PATH patterns — deliberately NOT in `.gitignore`, where check C would turn them into prose search tokens. 🟡 because on this repo's empty history it can only REFUSE (which it correctly does); it becomes ✅ the first time it audits real commits. |
| Static recompilation | `tools/recomp_substrate.py`, `game/recomp_seeds.json` → `generated/` | 🟡 | RE-02/C014/C017/I010/C020. Input-hash-checked shipping emission produces 365 resident roots → 889 functions plus 22 modules / 309 functions / 75 TUs; a separate digest refuses drift. FMV entry `0x800D6628` is binary/live proven. RE-14 classified LEVEL01 entry `0x800D12C4` (true overlay entry: module-ID header, +4 prologue, byte-identical residency, direct guarded boot `jal`, five descriptor copies) and seeded it as the first `overlay_seeds` LEVEL entry; the last live route advances to 8,320 commits and RE-16. Current psxport `bc8c8897` passes the exact generated-table, oracle and full static gates; its runtime presentation still awaits the already-recorded bounded visual revalidation. |
| Stock libcd command evidence | `tools/verify_cd_command.py` | ✅ | C012/I012. Exact retail instructions derive `0x80091DE4(cmd,param,result,async)`, sync `0x80091898`, service `0x80091310`, distinct ready/sync states, callbacks and results. The same tool segments multiple live Setloc/ReadN phases and classifies their controller traces against the mode-selected physical drive rate; selftest mutates the pre-sync call and produces both impossible and bounded answers. Real pre-fix/landed A/B also produces both answers. |
| Framework seam — runtime | `game/core/toystory2_runtime.*`, `game/core/legacy_game_interface.h` | 🟡 | `ToyStory2Runtime` is process-lifetime and explicitly legacy-backed, not direct. It owns RE-01 boot dispatch and RE-10 override installation; `LegacyGameRuntimeAdapter` supplies bc8c8897's required guest-VRAM picture policy from the one verified static answer while this route has no native producer. The base remains debt until remaining config facts and callbacks gain typed interfaces. |
| Framework seam — legacy facts | `game/core/game_config.cpp`, `game/core/game_hooks.cpp` | 🟡 | RE-01's boot group, RE-03's slots and RE-02's resident range remain in the bounded compatibility table because generic algorithms still read `Core::cfg`. `preserveVramBackdrop = 1` is now adapter input only; renderers ask the runtime policy. CD callables stay `0` rather than being miswired from `(path,dest)` into `(dest,lba,size)`. Hooks contain only neutral or fail-fast compatibility behavior. |
| Framework seam — field clock | `game/sync/` (`field_clock.*`), `game/core/toystory2_runtime.cpp` | ✅ | RE-10/RE-13/C013/C018/I013. The runtime installs the exact graphics-init override; after guest graphics init it arms host turns at the GPU field rate. Each turn samples pad, invokes registered guest VBlank `0x80039D60`, advances SPU and commits the captured queue through the neutral shared presentation fence. Real A/B changes no frame fences plus overflow into a current 8,320-commit route, stable title and no overflow. |
| Frame-fence/title verifier | `tools/verify_frame_fence.py` | ✅ | RE-13/C018/I014/C021. Reads the framework's authoritative RQ_MAX, classifies real pre/post traces and the shipping title callback, and color-discriminates two real 960x720 title captures from real black/transition controls. Selftest passes 12/12 including giant-flush, temporal-coupling, raw-present, wrong-current-address and wrong-current-consumer negatives. |
| Framework seam — recomp registry | `game/core/recomp_register.cpp` | 🟡 | The generated registry installs resident dispatch/index, all 22 module descriptors, and the override setter. CD/HLE/pad fields remain separate RE steps. |
| Process entry | `game/core/main.cpp` | 🟡 | Composition only: installs the process-lifetime `ToyStory2Runtime` and generated registry, initializes framework hardware owners, then delegates override installation and boot through `Core::runtime`. The executed route reaches verified crt0, guest main, FMV, renderer table, stable title, the seeded LEVEL01 entry 0x800D12C4 and the RE-16 unmapped-read boundary. |
| Build | `CMakeLists.txt`, `cmake/toystory2_port.cmake`, framework `external/psxport/tools/check_cpp_style.py` | 🟡 | Clang-only configure builds `toystory2_port`, one shared generated object set, and the independent-oracle boundary runner. CTest owns launcher, RE gates including frame-fence positive/negative controls, boundary compare, format, structure and tidy. Generated sources stay ignored and use their required wrap/aliasing/tail-call flags. Recorded and built framework commit `bc8c8897` passes 11/11. |
| Play launcher | `run.sh`, `tools/run.py` | 🟡 | The stable shell wrapper delegates resolution, focused 22-module provisioning, hash-checked emission, Clang build, RmlUi asset path, and launch to Python. The executable's direct headless route renders the stable title and advances past the seeded LEVEL01 entry to RE-16's unmapped-read boundary; the no-argument launcher still needs a bounded visual revalidation at the current pin. |
| Generated boundary gate | `tools/compare_recomp_boundary.py`, `tests/toystory2_recomp_boundary.cpp` | ✅ | Executes the identity-checked entry through shipping generated C and the independent Mednafen CPU oracle to first jal `0x80089344`: 34/34 state fields agree. Forced `a0` mutation gives one named mismatch, proving the comparator can report the opposite answer. The runner owns a complete framework `Game`, required because landed guest-instruction accounting reaches `Core::game->timing` on every generated block. Its CMake target explicitly depends on `oracle_trace`, so a clean documented build cannot pass compilation then make CTest refuse a missing stale artifact. |
| crt0 verification tool | `tools/verify_crt0.py` | ✅ | RE-01 / C009 / I008. Walks 43 instructions from pc0 through InitHeap, restored ra, gameMain and `break`; treats referenced post-break word `0x80082E10` as data, resolving Ghidra's truncation. Prints every field's exact instruction chain and compares 16 shipping/header constants. Portable selftest passes 8/8; genuine Tomba!2 cross-binary run passes 9/9. |
| Remaining asset packet pipeline (`.DAT` / `.ALL` / decoded `.RAW`) | — | ⬜ | `.RAW` framing and decompression are closed by C019/I015. The decoded per-chunk command IDs and all scene/collision structures remain unmeasured RE-15/RE-09 work; third-party taxonomy is only a locator until TS2's own loader verifies it. |
| Everything about the game itself | — | 🟡 | The executable substrate, 22-module registry, field delivery/fence, FMV path parsing, resident renderer table, first stable retail title and LEVEL01 entry are real. C021 proves the next boundary consumes an invalid model pointer through the real model-package table; writer provenance, gameplay, OT/packet layout, pad buffers and remaining HLE windows remain incomplete. |

## Known local costs, recorded rather than hidden

- **`tools/info.py`, `tools/catalog.py`, `tools/go_public.py` and `tools/code_scan.py` are COPIES**,
  because no hoisted engine exists for them in `external/psxport/tools/port/` yet (only `re_frontier.py`
  is hoisted). This is another copy of each, which is exactly the divergence that produced
  `re_frontier.py`'s four-times-fixed green-over-nothing bug. **Switch them to shims as soon as the
  engines are hoisted.** They were taken from `megamanx4/`, the newest copies, where the known bugs in
  `go_public.py` and `catalog.py` were already fixed — so the fixes are carried forward rather than
  re-imported broken.
- **`tools/base_fit.py` is a NEW tool in a game repo that answers a GENERIC question** ("where would this
  module be loaded"). Under the tooling-hoist decision it belongs in `external/psxport/tools/port/`; it
  is here because a game repo may not edit the framework. Hand it to the operator.
- **`psxport_smoke` cannot be built from a consumer tree** (framework defect, carried forward):
  `external/psxport/cmake/psxport.cmake` names its smoke source relatively, so with
  `-DPSXPORT_BUILD_SMOKE=ON` CMake looks for it under THIS repo. One-line fix upstream; not made here.
  `docs/issues/0001-*`.
- **The gitlink for `external/psxport` is NOT staged in this tree's index**, because the agent that
  created the tree is forbidden to `git add`. The working tree holds the correct checkout (`4d218e9f`)
  with both nested vendors populated, and `.gitmodules` is written; the operator must
  `git add .gitmodules external/psxport` in the first commit. Verify with
  `git -C external/psxport rev-parse HEAD` before trusting the pin.
