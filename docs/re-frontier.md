# RE Frontier — the ordered RE dependency chain toward a faithful port

Tracked by `tools/re_frontier.py` (a shim onto `external/psxport/tools/port/re_frontier.py` — consult it
FIRST; update it in the SAME commit that changes a step). This is the fine-grained companion to
`docs/codemap.md`: the codemap says *what subsystem exists*, this says *which ordered RE step is real
reverse-engineering vs a hack that jumped ahead*.

**Hard rule (no hacks / no fallbacks):** a `⛔ hack` status is DEBT, never an acceptable resting state.
It marks a shortcut standing in for absent RE and MUST be removed as its real mechanism lands.
`re_frontier.py hacks` is the debt list; `re_frontier.py next` tells you the next RE-ready step.

**`re-verified` MEANS FAITHFUL to the real target — not "the mechanism runs."** A step is `re-verified`
only when its OUTPUT matches the real game/binary on real data. An internal trace is a mechanism check,
NOT faithfulness.

**Fail fast & loud:** a failure must surface loudly, never silently fall back — unless the fallback IS
intended behaviour of the real target being reproduced.

Statuses: ✅ re-verified · 🟡 re-partial (honest gap) · 🔬 in-progress · ⛔ hack (debt, must remove) ·
⬜ todo · ➖ skip-by-design · ⏸ blocked (computed).

## THE STATE OF THIS PORT, 2026-08-12: NOTHING IS RE-VERIFIED. The repo is scaffolding

**Every entry below is `todo` or (computed) `blocked`. ZERO are `re-verified`, zero are `re-partial`,
zero are `skip-by-design`.** That is not modesty — it is the whole state of the port. There is no
recompiled substrate, no port binary, no native body, and no `GameConfig` value except this port's own
env-var names. `game/core/game_config.cpp` is all zeros with TODOs pointing back here, and that is the
honest value: a plausible-looking wrong address breaks boot in a way that reads as a framework bug.

**What IS measured is the DISC and the SUPPLY, never a port step.** The executable's identity, the
existence and byte count of 21 code overlays, a fitted (not resident) overlay base, the PSY-Q cohort, and
the `.RAW` container's framing — all in `docs/info/claims/`, each with its falsifier. Not one of them
fills a field or completes a step, and none of them may be quoted as progress.

**THE REAL FIRST STEP IS `RE-00`: stand up Ghidra on `SLUS_008.93`.** Not RE-01. This game has **no
decomp** (`docs/references.md`) — no symbol map, no function boundaries, no matching build — so the
decompiler is not a convenience here, it is the *entire* RE supply, and every other step below is blocked
behind it. The sibling ports could locate a value in a reference and then confirm it; this port must find
it first. Budget accordingly: this is a materially harder starting position than `vagrant` (CC0
`rood-reverse`, ~62% matched) or `megamanx4` (AGPL `sozud/mmx4`, byte-identical target).

The first *concrete* decompilation target is already identified and it is not crt0: the function that
references the overlay-name string group at VA `0x80022F84`..`0x80022FA8` (`level1.bin` at `0x80022F84`,
`level.bin` itself at `0x80022FA8` — xref both), i.e. the overlay loader (RE-03). It is the one that
turns this port's defining structural fact from a statistic into a mechanism.

## tooling

### RE-00 — a Ghidra project over SLUS_008.93: THE RE supply for this port
- status: todo
- deps:
- evidence:
- where: external/psxport/tools/decomp.sh (the framework's Ghidra headless wrapper); output belongs in the gitignored scratch/, never committed — it is derived from a copyrighted executable
- gap: Nothing stood up. This step exists as a step, rather than as an unstated assumption, because THIS PORT HAS NO DECOMP TO BORROW FROM: no symbol map, no function boundaries, no matching build (docs/references.md, docs/info/claims/006). The two sibling ports treat a decompiler as one tool among several; here it is the only source of every address the port will ever hold, which makes "is Ghidra actually running on these bytes" a real dependency and not boilerplate. Note the practical blocker recorded honestly rather than as a completed step: the session that measured this disc had NO Ghidra install available (no /opt/ghidra*), which is exactly why RE-03's mechanism is unanswered. Also note what must NOT be done in its place: hand-walking disassembly, or reasoning from a statistic to a mechanism. Both were tried on RE-03 and both correctly stopped short.
- notes: Two entry points are known from the PS-EXE header and the string table and are ready to be dropped into a fresh project: the crt0 entry pc0 = 0x80082D60, and the overlay-name string group at VA 0x80022F84..0x80022FA8 (level1.bin at 0x80022F84, level2.bin 0x80022F90, level3.bin 0x80022F9C, level.bin 0x80022FA8 — 0x80022F84 is the TABLE BASE, not the level.bin literal; xref both ends). `mateusfavarin/tsr`'s 14 named functions are NOT usable as addresses here — they belong to a different game's executable with no translation (docs/references.md).

## boot

### RE-01 — crt0 / boot layout: the GameConfig boot group for SLUS_008.93
- status: todo
- deps: RE-00
- evidence: Only the PS-EXE header and SYSTEM.CNF, which are INPUTS and not the group: pc0 = 0x80082D60 (file offset 0x73560), t_addr 0x80010000, t_size 0x00091800, d_size = b_size = 0 (so t_size covers .data and this game clears its own BSS), s_addr 0x801FFFF0, gp0 = 0 (the loader sets no $gp). SYSTEM.CNF: BOOT = cdrom:\SLUS_008.93;1, TCB = 4, EVENT = 16, STACK = 801FFF00 — which DISAGREES with the header's s_addr; SYSTEM.CNF wins at boot, and both are recorded in game/core/game_config.cpp rather than one being picked.
- where: game/core/game_config.cpp (bssZeroLo/Hi, stackTopBase/2, heapBase, heapSizePtr, heapBasePtr, gp, libcInit, gameMain, crt0)
- gap: Nobody has executed or decompiled the entry function. The framework consumes these eleven fields AS A GROUP, so writing the two or three a header hands you and zeroing the rest makes crt0_setup run a WRONG crt0 instead of refusing to run one — that is why the header facts sit in named constants and static_asserts and not in the struct. Measured but unexplained and therefore relevant here: BSS-looking addresses are materialised up to ~0x800CE1B0, i.e. well above the loaded image's end at 0x800A1800, so the BSS range is real and large; the clear loop's actual bounds are unmeasured. Expect the framework's crt0_setup to need the same generic fixes the sibling ports found (a stack bias field, treating heapSizePtr/heapBasePtr == 0 as "kept in registers only", and setting a1 for the BIOS InitHeap thunk) — those are FRAMEWORK changes and not this repo's to make.
- notes: The sibling ports' crt0 tooling (Tomba!2's re_crt0.py, megamanx4's verify_crt0.py) is the SHAPE to reuse — a tool that symbolically executes the entry function and prints the disassembly line behind every field, with a --selftest gating both classes and a cross-binary negative. It is not copied in speculatively; write it when RE-00 makes it runnable.

### RE-02 — recompiler seed set for SLUS_008.93
- status: todo
- deps: RE-01, RE-03
- evidence:
- where: game/recomp_seeds.json
- gap: Seeds are grown EMPIRICALLY from `[recomp-MISS] 0x800xxxxx` fail-fasts on a booting port, each with the rationale for how the address is reached. There is no booting port yet, so the file holds no addresses. It ALSO depends on RE-03, unlike the overlay-free sibling port: emit.py needs an overlay base per module and treats a missing one as a hard error, so 29.1% of this game's code cannot be emitted until the bases are confirmed. Never copy another game's seeds — a foreign seed landing inside real text SPLITS a function at an arbitrary offset, the emit SUCCEEDS, and the recomp is silently corrupt. There is no decomp here to lift a seed from even if that rule allowed it.
- notes:

## overlays

### RE-03 — the overlay loader: how the load base is computed, and how many slots are resident
- status: todo
- deps: RE-00
- evidence: THE EXISTENCE OF THE OVERLAYS IS MEASURED, four independent signals, denominators stated — docs/info/claims/002. 21 files, 245,148 B = 29.1% of code-bearing bytes: LEVEL01..LEVEL10/{LEVEL,LEVEL1}.BIN (20) + BITS/MEMORY.BIN; LEVEL00/LEVEL.BIN bears an overlay name but is a 4-byte placeholder holding no code. 23 of 274 scanned disc files contain a `jr $ra`: those 21, the boot exe, and FMV/FMV.BIN (68 of them, class UNRESOLVED — RE-04 below, NOT an overlay); every file other than those 23 has ZERO. 67.9-83.4% of each LEVEL*.BIN module's j/jal targets land inside the boot exe's .text (the boot exe's own figure is 91.5%), i.e. they are statically linked against the engine — that range covers the 16 LEVEL*.BIN files with any j/jal, the 4 stub LEVEL1.BIN files have zero (no evidence), and BITS/MEMORY.BIN reads 50.6% as its own figure. The file format is visible in the bytes: a u32 function count, then code, then an absolute-address table terminated by 0xFFFFFFFF (LEVEL07/08/09's LEVEL1.BIN are 24-byte stubs: count, two `jr $ra; nop` no-ops, terminator). A LOAD BASE IS MEASURED TOO, and it is deliberately NOT a resident base — docs/info/claims/003: tools/base_fit.py fits 0x800D1000 for 15 of 17 fitting modules with 75-100% hit and a 0.0% runner-up, and the modules' trailers plus 20 literals in the boot exe hold absolute 0x800Dxxxx pointers.
- where: game/core/game_config.cpp (overlaySlots — three slots, all zero), game/recomp_seeds.json (overlay_bases, overlay_base_patterns)
- gap: TWO THINGS ARE UNANSWERED AND BOTH NEED THE DECOMPILER, NOT MORE STATISTICS. (1) HOW the loader computes 0x800D1000: the constant appears NOWHERE in the boot exe — not as a lui/addiu pair (12,670 pairs folded over all 148,992 .text words, with 4,483 non-decoding words REPORTED as part of the denominator) and not as a literal word. Presumably an end-of-BSS linker symbol or a runtime allocation, which means the resident base may differ from the fitted one and would still fit. (2) WHETHER LEVEL.BIN AND LEVEL1.BIN SHARE ONE SLOT OR OCCUPY TWO: both fit 0x800D1000, which is a contradiction if they are ever resident together, and LEVEL06/LEVEL1.BIN's trailer holds pointers (0x800D12F4..0x800D1450) INSIDE LEVEL06/LEVEL.BIN's loaded span — which reads like cross-module calls between two simultaneously resident modules. Either reading fits the bytes and neither was picked. Also open: BITS/MEMORY.BIN's base is bounded only to 0x800D0000-0x800DA000 (its jal fit peaks at 0x800D6614, which is not a plausible aligned base, and its 25 header pointers bound it to 0x800C9FA5..0x800D95F4). AN OVERLAY IS KEYED BY ITS LOAD ADDRESS: a wrong base emits a whole module of correctly-decoded instructions at wrong addresses and every jal target, pointer test and router lookup then goes silently wrong, while the emit SUCCEEDS. emit.py's hard error on a missing base is the feature.
- notes: THE CONCRETE FIRST TARGET FOR RE-00: decompile the function that references the overlay-name string group at VA 0x80022F84..0x80022FA8. Supporting strings are located EXACTLY (VA = file offset - 0x800 + 0x80010000), and note that 0x80022F84 is the table base, NOT the `level.bin` literal: `level1.bin` at file 0x13784 = VA 0x80022F84, `level2.bin` 0x13790 = 0x80022F90, `level3.bin` 0x1379C = 0x80022F9C, `level.bin` 0x137A8 = VA 0x80022FA8 (12-byte stride; each occurs exactly once in the exe); 0x12518 holds the 15 directory stems `level01\level01` … `level05\level15` plus `bits\memory.bin`; `%s: not found` sits at 0x13768 and `%s: found` at 0x13778. Note that the engine names FOUR overlay slots per level while only two exist on the USA disc; `%s: not found` is its own handling of the absent two, not a sign of a missing file. Reopen-worthy discovery: any module that turns out to be relocated (fixup table) rather than absolutely linked, since base_fit cannot see those at all.

## cd

### RE-04 — CD load chokepoints and the loader's contract
- status: todo
- deps: RE-01
- evidence: The PSY-Q cohort (sys.c 1.129 / intr.c 1.76 / bios.c 1.86, docs/info/claims/004) says the framework's stock-libcd chokepoint set is the likely SHAPE. Nothing located.
- where: game/core/game_config.cpp (cdInit, cdCommand, cdSync, cdReadPrim, cdFileLoad, cdAsyncRead, …)
- gap: No address located and the loader MECHANISM is not confirmed. This step is more entangled here than in an overlay-free port: the CD path IS the overlay loader's path, so RE-03 and RE-04 will be worked together and a chokepoint found for one is evidence for the other. The disc's shape is known and is an input, not a finding: 300 files, 95.3% of the bytes in TOY2FMV/ (22 .STR MDEC videos, 3 .XA tracks of 58-60 MB each, a 26.9 MB DUMMY.DAT pad), a rigidly regular LEVEL01..LEVEL10 layout of two sub-areas each, and per-level VAB sample banks beside SFX/GLOBAL.{VB,VH}.
- notes: FMV/FMV.BIN (510,960 B) is the one file whose class is UNRESOLVED and it is recorded as unresolved rather than assumed: 3.2% code-plausible and no base fit (data), but 68 `jr $ra` words at 0.53/1k, which a pure data file normally has none of, and its out-of-.text jal targets pile 54% into one bucket at 0x8FFC0000 — a nonsense address, which is the tell for accidental opcodes. The lean is "an FMV index", not asserted. 510 KB is large enough that being wrong would matter.

## frame

### RE-05 — per-frame OT / packet-pool layout
- status: todo
- deps: RE-01
- evidence:
- where: game/core/game_config.cpp (otRegionBase/Stride, packetPoolBase/Stride, otBasePtr, poolPtrCur/Last, clearOtagR, putDrawEnv, drawSync)
- gap: Nothing located. ⬜ todo and NOT ➖ skip-by-design: no scope decision has been made for this port and NO measurement of this game's frame rate exists in this repo, so the zeros in the struct are un-RE'd values and not a decision. (The sibling megamanx4 marks this group ➖ on an explicit USER scope decision quoted in its own CLAUDE.md; nothing equivalent exists here, and inheriting another port's scope decision would be exactly the cross-tree contamination this workspace keeps paying for.)
- notes:

### RE-06 — pad driver buffers
- status: todo
- deps: RE-01
- evidence:
- where: game/core/game_config.cpp (padSlot0Buf, padSlot1Buf, padDriverFn, padSlotPtrTable, padSlotPtrStride)
- gap: Nothing located. The PSY-Q cohort says libpad rather than a custom SIO driver is the likely shape, which says what to look for, not where.
- notes:

### RE-07 — platform HLE windows (the hardware-sync primitives)
- status: todo
- deps: RE-01
- evidence:
- where: game/core/game_config.cpp (.hle)
- gap: Nothing located. ZERO MEANS "not RE'd, install nothing": initBuiltins() then registers no handler and says so, and a run that needs one hangs in the guest's real spin loop — the honest signal that the RE is outstanding. The windows are zero too, so register_() refuses everything, because this game has not stated its memory map yet and a window guessed from another game's map is how a handler lands on an unrelated function.
- notes: Kept as its own step rather than folded into RE-01: in the sibling tree leaving the HLE windows under the crt0 step made a done crt0 imply a done HLE.

## assets

### RE-08 — the .RAW container: the RNC ProPack method byte and a working decompressor
- status: todo
- deps:
- evidence: THE FRAMING IS CONFIRMED, the codec is not — docs/info/claims/005. A TS2 .RAW is a concatenation of RNC ProPack chunks whose standard 18-byte header has had its 4-byte magic ('RNC' + method) STRIPPED, leaving 14 bytes (be32 unpackedLen, be32 packedLen, be16 unpackedCRC, be16 packedCRC, u8 leeway, u8 packChunks), terminated by 0xFFFFFFFF. Discriminator was the packed CRC, not the lengths: LEVEL01/LEVEL.RAW reads chunks_scanned=39 crc_ok=39 crc_bad=0, CRC-16/ARC over each payload equalling be16@0x0A exactly, consuming 469,266 of 469,276 bytes and stopping on the sentinel. Instrument: tools/raw_probe.py, --selftest gates four classes and NEGATIVE C (one flipped payload byte -> crc_ok=38 crc_bad=1) proves the CRC check FIRES. Independently cross-confirmed: mouksx/Toy-Story-2-Modding's extracted `1.Andys House/level.raw` is byte-length-identical to our LEVEL01/LEVEL.RAW and its 39 sub-file lengths match our 39 decoded chunk lengths exactly and in order.
- where: no code yet — the eventual native asset path, and a decompressor for the port to read textures/level data with
- gap: TWO STATED GAPS, both printed by the instrument on every run. (a) The RNC METHOD byte (1 or 2) is unrecoverable from the stripped header and must be determined by ATTEMPTING decompression, never assumed. (b) The unpackedCRC is unchecked until a decompressor exists, so today's confirmation covers FRAMING only. Both are cheap to close: try both methods and check the unpacked CRC. temisu/ancient (BSD-2-Clause) supports both formats and ships test vectors; docs/references.md records the deliberate decision NOT to vendor it yet and why.
- notes: This step is unusual in this tree in having real external documentation behind it (docs/references.md) — and equally unusual in that the best-furnished TS2 tool in existence is useless for it, because it targets the PC build's .NGN container which is NOT on this disc (measured: 0 NGN hits over the 300 entries `tools/discdump.py list` returns).

### RE-09 — the scene / collision / model formats: .DAT, TERRAIN.ALL, .ANM
- status: todo
- deps: RE-00, RE-08
- evidence: A STARTING HYPOTHESIS ONLY, and it is other people's, not ours — docs/references.md. The strongest piece is a field-exact structural agreement between two independent REs of two different TT games: juanmv94 (from TS2 savestates) and mateusfavarin/tsr's docs/DAT.MD (from decompiled Toy Story Racer loaders) describe the same 0x20-byte object transform, offset for offset (s32 posX/Y/Z at 0x00, s16 rotX/Y/Z at 0x0C, s16 scaleX/Y/Z at 0x12, u32 mesh-stream offset at 0x1C), plus shared conventions: Euler rotation in PSX angle units, fixed-point scale with 0x1000 == 1.0, and DAT positions stored in HALF world units.
- where: no code yet
- gap: NOTHING HERE IS MEASURED AGAINST OUR BYTES. Two independent people agreeing is strong evidence about the ENGINE FAMILY and zero evidence about SLUS_008.93 specifically, and the sources carry documented per-game deltas (TS2 collision coordinates are s16 relative to the first point where Rascal used s32 absolute; TS2 vertices are s16 xyz + one rgb15 u16 = 8 bytes where A Bug's Life used 12 with rgb24; TS2 chains further 0x0100 blocks per collision object where Rascal and Bug's Life do not). There is also a build delta INSIDE this game: the TS2 demo's object struct is 4 bytes smaller than the retail one, so pin the retail USA build and never mix notes across builds. Confirm each field against our extracted files before any of it reaches code.
- notes: juanmv94's TravellersTalesPSXCollisionViewer is the best PSX-side document in existence for this engine and is UNLICENSED (all rights reserved) — read and learn only, never copy; it additionally embeds a lifted proprietary x86 blob and 54 copyrighted RAM images. Its own sample file corresponds to our LEVEL05/LEVEL1.RAW, i.e. it is PSX-targeted at our exact file layout, which is what makes it worth reading at all.
