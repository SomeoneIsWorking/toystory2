# Toy Story 2 — working rules and dynarec plan for this repository

This repository ports **Disney/Pixar Toy Story 2: Buzz Lightyear to the Rescue!** (PS1 USA,
`SLUS_008.93` / SLUS-00893), Traveller's Tales. It is one standalone title built on the shared
`external/psxport` framework.

## Current product contract

The product is one native/Lightrec hybrid:

- verified title-owned C++ implements deliberately native functions and subsystems;
- a per-`Core` Lightrec executor translates every remaining guest instruction on demand from the
  authenticated executable or currently loaded module; and
- native `superCall` executes the original guest body through that dynarec while suppressing only
  the current override for the duration of the call.

The gameplay target must not link, select, or fall back to an interpreter. An interpreter may exist
only in a separately built test target, including diagnostic tests. Unsupported guest behavior fails
with the exact guest PC; it never changes engines silently.

The repository has not implemented or verified this Lightrec gameplay product yet. The measured
native owners and old execution evidence below define migration inputs, not current dynarec status.

Offline/static guest translation is no longer a product path. Do not emit guest C/C++, generate a
title substrate, build or run the static product, or use it as the new oracle. Existing static-path
artifacts may supply already-recorded evidence only. After the representative-gameplay migration gate
passes, remove the generator, generated corpus/build rules, seed manifests, static dispatcher, and
generated-symbol methodology together rather than retaining a compatibility mode.

Read `external/psxport/CLAUDE.md` for the shared framework rules and
`../../shared/jit-common/docs/migration.md` for the portfolio migration contract. The latter wins over
stale static-recompiler language that remains elsewhere while psxport itself is being converted.

## Project authorities

`docs/project-goals.md` owns durable product intent. `docs/project-state.md` owns factual capability
coverage and current focus. `docs/issues/` owns atomic work, `docs/codemap.md` owns placement, and
`docs/re-frontier.md` owns the ordered binary-evidence chain. `tools/info.py` indexes the evidence
claims and instrument trust. This file owns the title-specific execution plan and invariants; it does
not promote old static execution evidence into dynarec evidence.

Start every non-trivial task with:

```sh
uv run --frozen python tools/info.py brief <words>
uv run --frozen python tools/re_frontier.py next
uv run --frozen python tools/catalog.py search <symptom>
```

## Migration sequence

1. Integrate the maintained, pinned Lightrec revision in psxport. Lightrec owns translated blocks,
   executable memory, and its cache; this title must not grow a second CPU engine or a title-local
   cache.
2. Map the identity-checked resident executable and streamed module bytes as runtime input. A fresh
   checkout must provision only authenticated game data and redistributable runtime metadata; it must
   not create guest source or object code.
3. Give each `Core` its own executor and override state. psxport synchronizes CPU state, memory,
   exceptions, HLE/device callbacks, and bounded exits at host work, frame suspension, interrupt, and
   thread-exit boundaries.
4. Key executable identity by image/module generation plus guest address. Module replacement, guest
   writes, DMA, savestate restore, and override install/remove/change invalidate every affected
   translated block or captured call decision.
5. Preserve the title's measured native owners and route every ordinary call, override, and scoped
   original call through the shipping dispatcher. Fix MIPS semantics in the shared Lightrec
   integration; a new native override is not a substitute for missing instruction behavior.
6. Pass the first discriminator below, then expand along reached control flow until representative
   interactive gameplay passes the complete migration gate. Only then delete the static pipeline.

## First implementation discriminator

From the exact `SLUS_008.93` image, the native/Lightrec gameplay product must reach coherent Andy's
Room within **120 host-owned frames** while all **22 streamed code modules** are eligible for execution
and replacement across their **two reused code slots**. The gate must prove:

- nonzero Lightrec block execution and no interpreter symbols or engine selector in the gameplay
  product;
- one reached resident native override and one reached streamed-module override;
- an override-bypassing `superCall` of each reached body through Lightrec;
- positive invalidation when either reused slot changes generation, plus a controlled negative showing
  that an unchanged generation retains its valid blocks;
- the existing finite frame owner reconciles 120/120 frames with no dropped layer or guest-VSync
  violation; and
- CPU, memory, interrupt, timing, and relevant device state at the bounded checkpoint agree with an
  independent emulator or separately built test oracle.

Boot, logos, title screens, FMV, or the 120-frame checkpoint alone do not complete the migration. The
representative-gameplay gate must then exercise real player input in Andy's Room, pause/unpause and
camera movement, streamed-code replacement, coherent world/actors/effects/HUD, audio, and sustained
frame progression on each released host architecture. Enhancements remain off for the faithful
baseline; widescreen and interpolated presentation are separate observable gates.

## Preserved binary and behavior facts

These facts remain valid migration inputs; they are not evidence that the dynarec is implemented.

### Identity, provenance, and repository shape

- The supported image is PS1 USA `SLUS_008.93`. The executable is 598,016 bytes with the identity
  recorded in `docs/info/exe-identity.txt`.
- No matching decompilation or symbol map is known. Every guest address must come from reproducible
  Ghidra/binary evidence on this exact image. Do not import facts from the PC, N64, or Dreamcast ports.
- Toy Story 2 remains one repository and one title. Its measured similarity to other available PSX
  binaries does not justify a shared `game/`, lineage layer, or `titles/` hierarchy.
- The PSX disc contains no `.NGN` asset pipeline: the project disc listing has 300 entries and zero
  `.NGN` hits. Third-party Traveller's Tales documentation is format evidence only and its platform
  and licence must be checked in `docs/references.md`.

### Runtime code images and invalidation identity

- The resident executable plus 22 loaded modules are the runtime executable corpus: 20 non-placeholder
  `LEVEL*/LEVEL*.BIN` alternatives, `BITS/MEMORY.BIN`, and mixed `FMV/FMV.BIN`. The 4-byte
  `LEVEL00/LEVEL.BIN` placeholder is not code.
- The LEVEL alternatives reuse the instruction-derived slot at `0x800D12C0`. `BITS/MEMORY.BIN` and
  `FMV/FMV.BIN` reuse the slot at `0x800D5D20`; FMV enters at `0x800D6628` (file offset `0x908`).
  Runtime identity therefore cannot be guest address alone.
- The first resident call boundary is `0x80089344`. IRQ resume `0x80088A2C` is a mid-function re-entry,
  not an ordinary function start.
- Model-table reset `0x80041F38` includes the branch-delay-slot increment at `0x80041FFC`; losing that
  increment repeatedly clears one slot and later corrupts a model pointer.
- `.RAW` payloads use Traveller's Tales' `DecompressRAW` LZ format, not RNC. The existing corpus check
  verifies both CRCs for 813/813 chunks across 46 files.

### Native ownership and reached behavior

- Retail VBlank callback `0x80039D60` and wait `0x8003FA68` explain the original field-progress
  dependency. The current finite frame owner services input, two resident display fields, direct
  deferred display work at `0x80021028`, audio, and one presentation commit. Linked libetc VSync
  `0x80088628` is forbidden across its exact `[0x80088628,0x80088770)` window.
- Guest-main initialization begins at `0x8007A9E8`; measured finite routing selects normal
  `0x8007B254` or alternate `0x8007B850`. LEVEL01 enters at `0x800D12C4`.
- Graphics initialization facts include `0x8003A218`/`0x8003A650`, SetGeomOffset `0x80083CD4`,
  SetGeomScreen `0x80083CF4`, and authored values `256/120/160`. Native ownership preserves their
  state effects without executing guest VSync.
- Retail pad initialization uses buffers `0x800CF8A0` and `0x800CF8C8`, stored in driver contexts
  `0x800A3E98` and `0x800A3F88` with stride `0xF0`; active-low Cross is `0xBF` and release is `0xFF`
  in the measured slot-0 packet byte.
- Existing bounded behavior reaches coherent Andy's Room, opens and leaves pause, and moves the
  camera under held Right. Its old static execution mechanism is not reusable proof for Lightrec, but
  the scenario and expected behavior remain valid discriminator inputs.
- Resident authored-state work has located camera record `0x800C1540`, producer `0x8002C848`, scene
  root `0x8002A070`, visible-list storage `0x800BB4D8`/`0x800C0AB0`, common owner `0x8002622C`, and
  dominant mesh leaf `0x800100E4`. The decoded pre-GTE mesh stream remains the correct source for a
  semantic native producer; post-GTE OT/packet replay is not.
- Widening the guest draw canvas from 512 to 684 pixels crosses fixed horizontal double-buffer parity
  in 1024-pixel VRAM and exposes wrapped/atlas columns. True widescreen must widen the title's
  projection/viewport/scissor and any proven culling owner at the semantic producer boundary; it may
  not stretch output or sample adjacent frames.

## Title-specific engineering rules

- Reverse-engineer first. A magic address, unnamed field, unknown structure, or suspected load base
  requires Ghidra and an exact-image measurement before implementation. Decode MIPS one word at a
  time in diagnostics; whole-range Capstone decoding can stop silently at the first undecodable word.
- Native overrides require a named ownership reason and a positive reach check. They preserve the
  guest ABI and may call the original only through the scoped Lightrec `superCall` contract.
- Keep `ToyStory2Runtime` as the framework-facing title owner and compose cohesive per-`Core`
  input/render/phase products. Do not grow legacy callback bags or a runtime god class.
- Guest-rendered 4:3 is the faithful baseline. Native producers read authored pre-GTE state.
  Widescreen is a deterministic projection change; interpolation uses explicit previous/current
  semantic identity and never mutates guest state.
- Diagnostics print denominators and a meaningful negative, refuse missing corpora, and prove both
  answers through the shipping seam they assess.

## Provisioning and shared framework

Game assets are never committed or packaged. Resolve the disc in the existing order: explicit CLI
argument, `PSXPORT_TS2_DISC`, `.env`, then an unambiguous repo-root `*.chd`; validate exact identity
before mapping executable bytes. Packaged first run will use the platform file picker and persist the
validated selection in OS user data.

`external/psxport` resolves to the one shared writable psxport checkout in this workspace or to a
private clone at `psxport.pin` in a standalone checkout. Framework execution, cache, memory, HLE, or
invalidation work belongs in psxport; title identity, native ownership, and Toy Story 2 behavior stay
here. Build outputs belong under `build/`; bounded logs and captures belong under the stable
gitignored `scratch/` children, never `/tmp`.
