# Codemap — what's where, what's done, what's missing

The orientation map: consult it at the START of a task to avoid re-deriving structure, and update it in
the SAME commit that lands or changes a subsystem. A stale map is worse than none — a subsystem is marked
done only when VERIFIED on real data, never to look better.

Companions: `docs/re-frontier.md` (the ordered RE steps: real vs hack), `docs/references.md` (the external
RE and format work, with licences, and the fact that no decomp exists), `docs/info/` (claims +
instruments), `docs/issues/` (what has been tried and ruled out).

**Status vocabulary:** ✅ verified on real data · 🟡 partial (gap named) · 🔬 in progress · ⬜ not started ·
❓ unresolved question · ➖ not applicable to this game · 🔴 regressed.

## Read this before anything else: the honest state, 2026-08-12

**This repo is scaffolding. The port does not run and nothing about the game is reverse-engineered.** What
exists is provisioning, the framework seam (compiling, all-zero), four measured instruments, and the
registries. There is no recompiled substrate, no `toystory2_port` binary, and no native body. **Every ✅
below is TOOLING** — a tool that was actually run in this tree, with its real output quoted in
`docs/info/instruments/`. Every ⬜ is real.

**AND THERE IS NO DECOMP OF THIS GAME.** No symbol map, no function boundaries, no matching build for
`SLUS_008.93`; the sibling ports each vendor one and this tree vendors nothing but the framework. Every
address this port will ever hold comes out of Ghidra on this executable — which is why
`docs/re-frontier.md` opens with RE-00 (stand up Ghidra) rather than RE-01. `docs/references.md` records
how that negative was established and that a search negative is weaker than a measurement.

**The defining structural fact, measured:** **Toy Story 2 STREAMS CODE OVERLAYS.** **21 files** hold
245,148 bytes = **29.1% of the game's code-bearing bytes** — `LEVEL01..LEVEL10/{LEVEL,LEVEL1}.BIN` (20)
plus `BITS/MEMORY.BIN`. (`LEVEL00/LEVEL.BIN` carries an overlay name but is a 4-byte placeholder holding
no code — not scannable, not counted.) Of 274 disc files scanned in full (24.5 MiB), exactly **23 contain
a `jr $ra`**: those 21, the boot executable, and **`FMV/FMV.BIN` (68 of them), whose class is UNRESOLVED**
(RE-04) and which is NOT an overlay. Every file other than those 23 has zero. The modules read
74.6–98.8% code-plausible against the boot exe control's 92.4%, and **67.9–83.4% of the 16 `LEVEL*.BIN`
modules' `j`/`jal` targets land inside the boot exe's `.text`** (the boot exe's own figure is 91.5%), so
they are statically linked against the engine and call into it; the 4 stub `LEVEL1.BIN` files have zero
`j`/`jal` (no evidence) and `BITS/MEMORY.BIN` reads 50.6% — its own figure, below the LEVEL band but far
above the `PATH*.BIN` trap case's 0%.
The trap case duly appeared and was rejected: `PAD/PATH*.BIN` reads 48.9–87.2% code-plausible with ZERO
`jr $ra` and ZERO `j`/`jal` — waypoint coordinates whose bytes decode as legal instructions. Full
denominators: `docs/info/claims/002-*`.

This is the Vagrant Story answer, not the Mega Man X4 answer. **Consequences:** `GameConfig` will need
overlay load bases, the RAM map is NOT static, and `emit.py` cannot emit anything until RE-03 lands
(a missing overlay base is a hard error there, deliberately). A base IS measured — `0x800D1000`, 15
modules, 0.0% runner-up (`claims/003-*`) — and **it is a FIT, not a resident base**: the constant appears
nowhere in the boot exe, and whether `LEVEL.BIN` and `LEVEL1.BIN` share one slot is unresolved.

**BLIND SPOTS of everything above, stated because the method cannot see past them:** the census detects
PLAIN R3000A code, so it asserts *"TS2 streams plain absolutely-linked overlays and nothing else on the
disc is plain code"*, NOT *"there is no other code on the disc"* — compressed or packed code would read as
a texture. 26 files / 525 MB of `TOY2FMV/` (95.3% of the disc) was never scanned; that is the largest
unscanned mass and "unmeasured" is not "absent". Fragments under 64 bytes are invisible by construction.

## The two halves

| | |
|---|---|
| `external/psxport/` | the PSX-generic framework (submodule, pinned `4d218e9f` — the same pin as every other tree in this workspace): MIPS→C recompiler, runtime substrate, GTE/SPU/MDEC/CD/GPU backends, SDK HLE, SBS differential harness, SDL_GPU renderer. **Not ours** — fix framework bugs upstream in the workspace dev clone, never in this submodule. |
| `game/`, `tools/`, `generated/` | this port: the seam, the RE, the provisioning, and (eventually) the recompiled substrate. |

There is no third directory. `external/` holds exactly one submodule, because **there is no decomp to
vendor** and the one thing worth vendoring later (`temisu/ancient`, BSD-2-Clause, the RNC codec) is
deferred until RE-08 needs it — `docs/references.md` records that decision and its reasons.

## Subsystems

| subsystem | where | status | notes |
|---|---|---|---|
| Disc resolution | `tools/resolve_disc.py` | ✅ | One implementation of CLI arg > `$PSXPORT_TS2_DISC` > `.env` > a `*.chd` in the repo root; refuses (exit 2) naming all four sources rather than returning empty, and refuses when a configured path does not exist instead of falling through to a different disc. `run.sh`, `extract_exe.py`, `extract_disc_files.py` and `discdump.py` all go through it. |
| Disc listing | `tools/discdump.py` | ✅ | Thin wrapper over the framework's own `discdump`, resolved through `$PSXPORT_DIR` and built into this repo's gitignored `scratch/build/discdump-<checkout hash>/` — never into the read-only submodule. Refuses (exit 2) when `discdump` is missing, the disc is unresolvable, or a read yields zero entries. Verified here: 300 entries read from the retail USA CHD — and 300 is the denominator every doc in this tree must quote for a disc-wide negative (`grep -ci NGN` → 0 of 300). |
| Disc → executable provisioning | `tools/extract_exe.py` | ✅ | Extracts `SLUS_008.93` (598,016 B), prints the PS-EXE header, and checks SHA-1 + size against `docs/info/exe-identity.txt`. Real output: MATCH `f90c9cd6…`. **Weaker than the sibling ports' equivalent and it says so on every run:** the expectation is OUR OWN measurement, because no decomp declares a target hash for this executable. Refuses (exit 2) rather than passing quietly if the identity file is missing. |
| Flat scan corpus | `tools/extract_disc_files.py` | ✅ | 300 listed → 26 TOY2FMV skipped → 274 extracted **and size-verified** → 0 failed (24.5 MiB in `scratch/flat/`). Refuses (exit 2) on zero extracted; prints the TOY2FMV denominator gap every run. |
| Code / overlay census | `tools/code_scan.py` | ✅ | The three-signal detector behind the overlay finding — a VERBATIM copy of `megamanx4/tools/code_scan.py` (identical logic and thresholds), which imports the calibrated code/data discriminator from `external/psxport/tools/exe_similarity.py` so there is ONE calibrated copy. `--selftest` run here on TS2's own data: POSITIVE 99.2% at pc0, NEGATIVE 0.1% on a VAB sample bank, PASS. `--census` refuses on a zero-match glob. The same unmodified code gave megamanx4 the OPPOSITE structural answer, which is the best evidence it measures files rather than expectations. |
| Overlay load-base fit | `tools/base_fit.py` | ✅ | The instrument behind `claims/003`. `--selftest` run here: 10 of 10 `LEVEL*/LEVEL.BIN` fit a base, **exactly** 9 at `0x800D1000` with `LEVEL09` at `0x800D2000`; 150 asset files and the 5 `PATH*.BIN` trap files all rejected; PASS. A **DERIVATION gate** (added 2026-08-12 after review) is what makes the reported base evidence rather than an echo: the `LEVEL09` disagreement, a re-score at the reported base that must equal the claimed hit count, and a decoy base one page up that must score strictly worse. Shown catching the defect it was written for — splicing `h1, b1 = scores[0][0], 0x800D1000` into `fit()` (report the recorded slot for every file, keep the true argmax's hit count) previously printed PASS and now exits 1 with 4 FAILs. Refuses (exit 2) on a missing corpus AND on a missing boot exe (without a `.text` range every file would appear to fit something). Belongs in `psxport/tools/port/` under the tooling-hoist decision; lives here because a game repo may not edit the framework. |
| `.RAW` container probe | `tools/raw_probe.py` | ✅ | The instrument behind `claims/005`. `--selftest` run here: 39/39 chunk CRCs on `LEVEL01/LEVEL.RAW` to a sentinel; a `.DAT` (exit 2), a truncation (exit 1) and a one-byte payload flip (39 chunks, `crc_ok=38 crc_bad=1`, exit 1) all rejected; PASS. That third negative is what proves the CRC check fires. **All four classes are now asserted on `report()`'s own EXIT STATUS, not on `probe()`'s internal tuple** (fixed 2026-08-12 after review): `report()` returned 0 whenever `crc_bad == 0`, so a truncated `.RAW` — a valid prefix that never reaches the sentinel, 3,450 bytes unexplained — exited 0 in the machine-readable channel while the selftest's tuple assertion passed. A walk that stops anywhere but the sentinel is now exit 1, and reverting that check makes NEGATIVE B fail. Framing only — it does not decompress, and the RNC method byte is unrecoverable from the stripped header (RE-08). |
| Registries | `tools/re_frontier.py` (shim), `tools/info.py`, `tools/catalog.py` | ✅ | The RE-frontier tracker is a SHIM onto the shared engine at `external/psxport/tools/port/re_frontier.py` — this repo grows no fork of it. Verified: `re_frontier.py check` parses 10 entries, `stats` reports 2 todo / 8 blocked / **0 re-verified**. `info.py` and `catalog.py` are COPIES (no hoisted engine exists for them yet) — see the known costs below. |
| Publication audit | `tools/go_public.py` | 🟡 | Copied from `megamanx4/tools/` (the already-fixed copy: it used to print "clean ✓ — ready to publish" over zero blobs scanned). CONFIG retuned for this game: `*.chd`/`*.cue`/`*.ecm`/`*.pbp` and `SLUS_008.93`/`SLUS_00893` as copyright PATH patterns — deliberately NOT in `.gitignore`, where check C would turn them into prose search tokens. 🟡 because on this repo's empty history it can only REFUSE (which it correctly does); it becomes ✅ the first time it audits real commits. |
| Static recompilation | `game/recomp_seeds.json` → `generated/` | ⬜ | **Not started, blocked on RE.** The seed file is empty in all five keys. Two prerequisites here rather than one: seeds (RE-02) AND the overlay load bases (RE-03), because 29.1% of this game's code is in modules `emit.py` refuses to place without a confirmed base. |
| Framework seam — config | `game/core/game_config.cpp` | ⬜ | Compiles; every guest address is `0` with the frontier step named. The only non-zero values are port facts, not RE: `discEnvVar`, `cardEnvVar`/`cardDefaultPath`, `paceQuota = 1`, `preserveVramBackdrop = 1`, `windowTitle`. The PS-EXE header facts (entry `0x80082D60`, text `0x80010000+0x91800`, the two DISAGREEING stack values `0x801FFFF0` header vs `0x801FFF00` SYSTEM.CNF) and the fitted overlay base are recorded as `static constexpr` + `static_assert`s, deliberately NOT wired into the struct — the framework consumes the boot group AS A GROUP, and an overlay is keyed BY its load address. |
| Framework seam — hooks | `game/core/game_hooks.cpp` | ⬜ | Compiles. Neutral bodies where "nothing is owned" is the correct semantic; fail-fast `abort()` bodies for every framework path this port has not stood up. `bootInit` refuses rather than dispatching `gameMain == 0`. |
| Framework seam — recomp registry | `game/core/recomp_register.cpp` | ⬜ | Deliberately UNWRITTEN and excluded from the compiling target — it is the one TU that names `generated/` symbols. It `#error`s under `TS2_HAVE_SUBSTRATE` (which the port target defines) so a substrate cannot appear without someone writing the real registry. |
| Process entry | `game/core/main.cpp` | 🟡 | Compiles. Framework bring-up in the standard order (GTE/MDEC/SPU/GPU/CD/HLE/pad), then `native_boot_run`. Never executed: there is no binary to run. |
| Build | `CMakeLists.txt`, `cmake/toystory2_port.cmake` | 🟡 | `psxport` (framework lib) and `toystory2_seam` (OBJECT lib over the three seam TUs — the gate that runs today) configure from a bare clone. `toystory2_port` is configured ONLY when `generated/rec_sources.cmake` exists, with a loud configure-time STATUS naming RE-01/RE-02/RE-03. `PSXPORT_DIR` defaults to the submodule and is spelled exactly once; both of psxport's nested vendors are checked with the exact per-path init command in the failure message. |
| Play launcher | `run.sh` | 🟡 | The USER's; agents must never run it. Does every step that is real today and then STOPS with exit 3, naming RE-01/RE-02/RE-03 and the absence of a decomp as what blocks the recompile. |
| Gate on a built binary | — | ⬜ | **Deliberately absent.** A gate drives an ALREADY-BUILT binary and there is none. When it is written, on `Tomba2Engine/tools/gate.py`'s shape: it neither builds nor extracts, it drives the binary, it prints its own denominator every run, its refusals exit 2 rather than 0, and it asserts the ADVANCE past the prologue plus the END STATE — never an absolute frame number. No boot measurement may be quoted before this exists. |
| crt0 verification tool | — | ⬜ | Not written, deliberately. The sibling ports' `re_crt0.py` / `verify_crt0.py` (symbolically execute the entry function, print the disassembly line behind every field, `--selftest` gating both classes plus a cross-binary negative) is the SHAPE to reuse — write it when RE-00 makes it runnable, not speculatively. |
| Asset pipeline (RNC / `.DAT` / `.ALL`) | — | ⬜ | Nothing built. The container framing is confirmed (`claims/005`) and the codec is not (RE-08); the scene/collision formats are a third-party hypothesis and nothing more (RE-09). `temisu/ancient` (BSD-2-Clause) is the intended supply and is deliberately not vendored yet. |
| Everything about the game itself | — | ⬜ | crt0, the overlay loader, the CD path, the frame loop, the pad buffers, the HLE windows: **not started.** `docs/re-frontier.md` is the ordered list, and RE-00 (Ghidra) gates all of it. |

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
  `external/psxport/cmake/psxport.cmake` names `tools/smoke/psxport_smoke.cpp` relatively, so with
  `-DPSXPORT_BUILD_SMOKE=ON` CMake looks for it under THIS repo. One-line fix upstream; not made here.
  `docs/issues/0001-*`.
- **The gitlink for `external/psxport` is NOT staged in this tree's index**, because the agent that
  created the tree is forbidden to `git add`. The working tree holds the correct checkout (`4d218e9f`)
  with both nested vendors populated, and `.gitmodules` is written; the operator must
  `git add .gitmodules external/psxport` in the first commit. Verify with
  `git -C external/psxport rev-parse HEAD` before trusting the pin.
