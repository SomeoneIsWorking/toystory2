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

## THE STATE OF THIS PORT, 2026-08-24: headless boot renders the stable retail title

**RE-00 supplies Ghidra and RE-01 proves the complete crt0 group.** `tools/verify_crt0.py` resolves the
entry decompiler's bad-data truncation from the instruction stream and gates every shipped boot field.
RE-03 proves the two resident overlay slots and wires every known code module's load base. RE-02 now
emits the executable plus all 22 proven modules, links the real port, and matches the independent CPU
oracle at the executable's first call in all 34 state fields. The first live dispatch miss identified
the IRQ resume at `0x80088A2C`; its binary shape proves it is a mid-function re-entry, and the landed
emitter now discovers and dispatches that seed class directly. Boot advances through interrupt
registration, disc opening and serviced stock-libcd results. RE-04 now instruction-verifies
`0x80091DE4(cmd,param,result,async)`, its sync loop `0x80091898` and interrupt-result service
`0x80091310`. The honest boundary was one layer lower: psxport's CDC made the following-sector INT1
available immediately rather than at the mode-selected drive rate. Pinned psxport `3418a79b` removes
that race without game HLE. RE-10 then names the first render blocker: graphics init registers guest
VBlank `0x80039D60`, but the seam delivered no host field turns, so wait `0x8003FA68` could never
observe its counter or deferred-work flag advance. The game-local field clock now invokes the exact
registered callback at the GPU rate. Live headless A/B changes zero presents plus a 30-second watchdog
into non-black legal/ESRB frames at presents 30, 120 and 900. Boot then loads FMV/FMV.BIN at
`0x800D5D20`, calls file+`0x908` (`0x800D6628`) and executes emitted FMV code. Pinned psxport
`d2266f4b` first carried A0:0x25 and generic lowering of the resident renderer's internal 32-way
computed jump table; current pin `8611d756` retains both under the full Clang/static gate. The last
live retail trace completes its first path, both formerly missing renderer slots execute,
and RE-13 closes the title-owned frame fence: the field callback commits the shared captured queue
once per display field instead of bypassing it through the lower-level presenter. RE-14 proves and
seeds LEVEL01 entry `0x800D12C4`. RE-16 then traces the invalid model pointer to a false overlay-data
function root that split the model-table reset from its branch delay-slot increment. Corrected generic
emission clears slot 9 before arena reuse, reloads a fresh package, and advances through field 10,303
without a fatal or recompilation miss.

**What IS otherwise measured is the DISC, not a game implementation.** The executable's identity, the
existence and byte count of the original 21 plain overlays, the now-proven 22nd FMV code module, the old coarse fitted base, the PSY-Q cohort, and the
`.RAW` container's framing and codec — all in `docs/info/claims/`, each with its falsifier. C009 fills the crt0
group and C010 fills the overlay map; the remaining disc measurements do not fill shipping fields.

**RE-00 now stands up Ghidra on `SLUS_008.93`.** This game still has **no
decomp** (`docs/references.md`) — no symbol map, no function boundaries, no matching build — so the
decompiler is not a convenience here; it is the *entire* RE supply. With RE-00 verified, that supply no
longer blocks the first code RE. The sibling ports could locate a value in a reference and then confirm
it; this port must find it first. Budget accordingly: this is a materially harder starting position than `vagrant` (CC0
`rood-reverse`, ~62% matched) or `megamanx4` (AGPL `sozud/mmx4`, byte-identical target).

RE-14 classified and seeded `0x800D12C4` as LEVEL01's true entry. RE-16 is now complete: resident
model consumer `0x800426E0` dereferenced `0xEDA4F893` because the generated reset repeatedly cleared
only table slot 0. Mixed FMV payload words had been promoted as calls to delay slot `0x80041FFC`,
partitioning away retail's `addiu a1,a1,4`. The generic partition fix removes five impossible roots;
the live route observes slot 9 clear/reload and crosses the retired fault. The next product frontier is
RE-06 pad/input discovery and an interactive gameplay witness. RE-05's OT/packet-pool layout remains
independently TODO; visible boot/title frames do not close it.

## tooling

### RE-00 — a Ghidra project over SLUS_008.93: THE RE supply for this port
- status: re-verified
- deps:
- evidence: C008, I005 and issue #5. From verified SLUS_008.93 sha1 f90c9cd6b4fc9845adfe34e306b7df393bf9154c, tools/ram_image.py placed exactly 595,968 bytes at [0x80010000,0x800A1800). A fresh Ghidra 12 MIPS:LE:32:default import (ts2boot_re00) reported Analysis succeeded. python3 tools/re_xref.py --project ts2boot_re00 --selftest passed 10/10 fold controls plus 5/5 independent Ghidra/fold controls. external/psxport/tools/decomp.sh decomp then emitted one 24-line function for header entry 0x80082D60; Ghidra created FUN_80082d60 and warned that control flow truncates at unresolved data, which is a limitation rather than suppressed.
- where: tools/ram_image.py (header-driven 2 MiB image + placement manifest); external/psxport/tools/decomp.sh (authoritative import/decompile); tools/ghidra_xref.py + tools/re_xref.py (cross-method evidence gate); all derived output stays in gitignored scratch/
- gap: The RE supply is working; Ghidra types, function boundaries and names remain hypotheses until each downstream step verifies them. RE-01 resolved the entry truncation independently with a symbolic instruction walk, and RE-03 independently checks the loader decompile against exact instruction chains.
- notes: Reproduce from the repo root: python3 tools/ram_image.py; external/psxport/tools/decomp.sh import scratch/ghidra/ram-boot.bin ts2boot; python3 tools/re_xref.py --selftest; external/psxport/tools/decomp.sh decomp ts2boot scratch/decomp/entry.c list 0x80082D60. A fresh-project proof may use another project name and --project. The xref wrapper is Python because run.sh is the only shell script this repo may own. Issue #5 records why prior Ghidra xref residue was not trusted: PyGhidra 3 did not expose currentProgram through globals(), so the old postscript dispatch ran only the standalone fold.

## boot

### RE-01 — crt0 / boot layout: the GameConfig boot group for SLUS_008.93
- status: re-verified
- deps: RE-00
- evidence: C009 and I008. tools/verify_crt0.py --check over verified SLUS_008.93 sha1 f90c9cd6b4fc9845adfe34e306b7df393bf9154c walked 43 instructions: BSS [0x800A1070,0x800D12C0), stack word at post-break data 0x80082E10 -> sp/fp 0x80200000 with bias 0, reserve word 0x800A0764, heap base 0x800D12C0 and size 0x126D40, gp 0x800A0CD8, A(39h) InitHeap thunk 0x80089344, gameMain 0x8007A9E8, optional heap globals absent. Shipping comparison passed 16/16. --selftest --cross ../Tomba2Engine/scratch/bin/tomba2/MAIN.EXE passed 9/9.
- where: tools/verify_crt0.py; game/core/game_config.cpp (complete measured boot group)
- gap: Complete. The crt0 group is not the substrate; RE-02 is now ready because RE-03 has supplied the streamed modules' verified resident bases.
- notes: Reproduce the portable gate with python3 tools/verify_crt0.py --check and python3 tools/verify_crt0.py --selftest. In the shared workspace, add --cross ../Tomba2Engine/scratch/bin/tomba2/MAIN.EXE for the genuine second-binary negative. The verifier resolves Ghidra truncation by proving break 0x80082E08 ends control before the referenced inline word 0x80082E10.

### RE-02 — recompiler seed set for SLUS_008.93
- status: re-partial
- deps: RE-01, RE-03
- evidence: C014, I010 and I011. tools/recomp_substrate.py identity-checks SLUS_008.93, re-runs the RE-01/RE-03 shipping gates, and drives the shipping emitter over 22 proven modules / 756,108 file bytes: 360 binary-rooted resident seeds -> 884 functions and 22 modules -> 309 functions across 75 generated TUs. Five prior roots were overlay-data coincidences targeting resident branch delay slots and are structurally impossible function entries. The old C011 denominator is falsified, not silently edited. tools/compare_recomp_boundary.py executes generated crt0 and the independent Mednafen CPU oracle to first jal 0x80089344: 34/34 state fields agree with a forced named mismatch. IRQ resume 0x80088A2C remains solely main_reentry. FMV entry 0x800D6628 and LEVEL01 entry 0x800D12C4 are overlay seeds backed by exact load/call evidence. The obsolete renderer diagnostic seed was removed because it split the function the generic emitter must analyze.
- where: tools/recomp_substrate.py; tools/compare_recomp_boundary.py; tests/toystory2_recomp_boundary.cpp; game/recomp_seeds.json; game/core/recomp_register.cpp; generated/ is gitignored
- gap: Seed completeness remains empirical. The corrected candidate passes the FMV parser, resident renderer's exact 32-way internal table and title frame fence without a game seed, executes LEVEL01, clears/reloads model slot 9, and continues through field 10,303 without a fatal or miss. The bounded run does not prove indefinite stability or interactive gameplay.
- notes: Never copy another game's seeds. A foreign address can split a real function at an arbitrary offset while emission still succeeds. Reproduce the committed evidence with `python3 tools/recomp_substrate.py --selftest`, build `toystory2_recomp_boundary`, then run `python3 tools/compare_recomp_boundary.py --selftest --oracle build/psxport_build/tools/oracle/oracle_trace --runner scratch/bin/toystory2_recomp_boundary`.

### RE-11 — FMV ISO-9660 path parser and shared BIOS toupper
- status: re-verified
- deps: RE-02, RE-03, RE-10
- evidence: C016, resolved issue #13 and scratch/decomp/re11-fmv-parser.c: retail FMV 0x800D8E48 calls executable Sony leaf 0x80082E5C; that leaf selects A0:0x25, and return 0x800D8E50 stores v0 as the normalized path byte. Current pin 8611d756 retains shared toupper and passes the complete Clang/static gate. The last bounded retail trace on d2266f4b observes the exact 15 normalized bytes of `toy2fmv\acti.str` plus 62 same-caller calls, then passes the renderer and frame fence to 0x800D12C4.
- where: tools/verify_fmv_boundary.py; shared implementation belongs in psxport bios_libc_string, not this repo
- gap: Complete for the retail FMV parser boundary. A different executable leaf/table selector, changed path-parser return site, or failure of the landed real-disc gate reopens it.
- notes: FMV and MEMORY remain alternative contents of RE-03 one arena. The fresh ts2fmv_re11 Ghidra image injects FMV only at the already-proven shared base; it is evidence for this call chain, not a second resident slot.

### RE-12 — resident renderer computed jump table
- status: re-verified
- deps: RE-02, RE-11
- evidence: C017 and resolved issue #14. Exact retail words build table base 0x800103EC, mask the selector with 31, scale by eight and `jr` (not `jalr`) into 32 consecutive `j`+nop trampolines inside function 0x800100E4. Observed slot 0x8001040C jumps to body label 0x80014838 while preserving outer ra=0x80026568. Generic computed-offset recovery preserves its `lui`/`ori` base across ordinary branches and emits the exact ordered 32-slot local switch without a renderer seed. `tools/verify_render_reentry.py --selftest` passes 13/13 classes and the real corrected log crosses RE-16 through field 10,303.
- where: tools/verify_render_reentry.py; game/recomp_seeds.json (no renderer table seed); generic fix belongs in psxport's emitter
- gap: Complete for this table. A changed table construction or case set, a regenerated switch that is not exactly the 32 retail slots, either former dispatch miss, or failure to reach C021's reset/continuation evidence reopens it.
- notes: Absence of a stored RAM pointer was not classification evidence; executable table construction and control-transfer opcodes decide the class.

### RE-13 — guest-owned frame fence and first stable title
- status: re-verified
- deps: RE-10, RE-12
- evidence: C018, I014 and resolved issue #15. The pre-fix retail trace has 6,613 queue flushes, 5,518 explicit re-emits, no commits, maximum flush 1,231, then accumulates 65,337 + 1,210 beyond unchanged RQ_MAX 65,536. `tools/verify_frame_fence.py` rejects a single giant producer as the same class. The title-owned one-field callback now calls the neutral shared `game->presentation.commit(core,1)` fence without opting into a temporal decorator. Pinned psxport d2266f4b retail evidence advances through LEVEL01 for 8,320 commits, maximum flush 1,472, maximum captured field 3,096 and no overflow. Its 960x720 presents 1,500 and 2,100 are stable Toy Story 2 titles; the shipping color-family discriminator rejects real presents 900 and 2,400. Twelve selftest classes exercise both answers, including direct temporal-decorator coupling, lower-level presentation-bypass, wrong-consumer and wrong-address negatives.
- where: game/sync/field_clock.cpp; tools/verify_frame_fence.py
- gap: Complete for the guest-loop frame fence and first title. A cap increase, primitive drop, return to raw gpu_present, per-field capture reaching capacity, or title discriminator accepting either real negative reopens it.
- notes: This is title-owned boundary wiring through the shared one-renderer path, not a title queue size and not a game-local presenter. Generated code remains untouched.

### RE-14 — LEVEL01 target 0x800D12C4
- status: re-verified
- deps: RE-03, RE-13
- evidence: C020. CLASSIFIED TRUE OVERLAY FUNCTION ENTRY and seeded (`game/recomp_seeds.json` overlay_seeds `LEVEL01__LEVEL`). Four independent signals: (1) every LEVEL module's first word is a sequential module ID so +4 is content start; (2) LEVEL01 +4 is a genuine entry prologue (addiu sp,-0x18 / sw ra / sw s0 / lw s0); (3) the miss RAM dump holds the loaded file byte-identical at the slot for 13,336/13,868 bytes — not corruption; (4) boot text carries a direct guarded `jal 0x800D12C4` and five 0x98-stride RAM descriptors store it as their entry value — not a mid-function resume point.
- where: game/recomp_seeds.json; generated/ov_level01__level_disp.c (module funcs 18->20); tools/verify_frame_fence.py + tools/verify_render_reentry.py updated to the moved boundary
- gap: none for classification. The historical post-seed frame-fence trace ran past the former entry miss to the later `0xEDA4F893` pointer fault at 8,320 commits, max captured field 3,096 under cap 65,536, with title frames measured at presents 1500/2100 and controls at 900/2400. RE-16 subsequently resolved and crossed that fault; RE-14 remains complete independently.
- notes: Reproduce with the headless route (`PSXPORT_NOPACE=1 PSXPORT_DEBUG=rqflush,ts2-field PSXPORT_PRESENT_SHOT_AT=900,1500,2100,2400 ./scratch/bin/toystory2_port scratch/bin/toystory2/SLUS_008.93`) and `python3 tools/verify_frame_fence.py --check`.

### RE-16 — the post-seed terminal fault: unmapped read16 @ 0xEDA4F893 via func_800426E0
- status: re-verified
- deps: RE-14
- evidence: C021 and resolved issue #17. Fault registers prove package index 9/model index 0: table slot 0x800C728C supplies package 0x8013B770 and package+8 supplies payload bytes 93 F8 A4 ED to 800426E0. Writer watches show the entire old package prefix is later overwritten by disc/DMA. Retail reset 80041F38 has bnez at 0x80041FF8 with delay-slot addiu a1,a1,4 at 0x80041FFC, but mixed FMV payload words 0x0C0107FF spuriously seeded that delay slot as a resident function. The split generated loop wrote zero to table base 128 times without advancing. The generic emitter rejects structurally impossible delay-slot roots; its RED whole-pipeline control changes from one increment to 3/3 and the direct Clang emitter suite passes 48/48. Regenerated TS2 has 360 roots -> 884 functions, no gen_func_80041FFC, and the intact reset increment. The bounded real writer trace observes slot9 0x8013B770 -> zero -> fresh 0x8012ED8C and continues through field 10,303 with no fatal or recomp-MISS. tools/verify_model_table_reset.py gates 7/7 positive/negative classes and the real log.
- where: tools/verify_model_table_reset.py; tools/verify_render_reentry.py; scratch/logs/re16-reset-fixed.log; scratch/decomp/re16-pointer-chain.c, re16-model-table-producer.c and re16-model-relocator.c; docs/issues/0017-*
- gap: none for the stale-table producer and retired 0xEDA4F893 boundary. The single 180-second continuation ended by its bound without reaching another fatal, so it does not prove indefinite stability or interactive gameplay.
- notes: RE-16 is complete without a pointer substitution, skipped access, generated-C edit, or title/address special case. The next product frontier is RE-06 pad-buffer/input discovery plus a bounded interactive gameplay witness; RE-05 OT/packet attribution also remains independently TODO.

## overlays

### RE-03 — the overlay loader: how the load base is computed, and how many slots are resident
- status: re-verified
- deps: RE-00
- evidence: C010, C014 and I009. The established LEVEL/MEMORY contract remains: LEVEL alternatives at 0x800D12C0 and BITS/MEMORY at 0x800D5D20. Exact retail words now additionally prove 0x8003EEAC loads `fmv\fmv.bin` to the same 0x800D5D20 slot and 0x8003EEC4 immediately calls 0x800D6628. That address is file+0x908 inside the 510,960-byte retail module and begins exact prologue word 0x27BDFF10. A live boot independently reaches the same entry with FMV bytes resident. `--selftest` remains 10/10 including the forced opposite slot answer.
- where: tools/overlay_map.py; game/core/game_config.cpp (physical LEVEL and MEMORY/FMV slots only); game/recomp_seeds.json (22 module placements and FMV entry)
- gap: Complete for the proven 22-module placement. FMV and MEMORY are mutually exclusive contents of the same physical slot; no duplicate GameConfig slot was added. A relocated module or a second level load on one invocation would reopen this step.
- notes: Reproduce with `python3 tools/overlay_map.py --check` and `python3 tools/overlay_map.py --selftest`. Ghidra evidence: `python3 tools/re_xref.py --project ts2boot_re00 scratch/decomp/re03-xrefs.txt 80022F84..80022FAC 80021E08 800D12C0 800D5D20`; decompile 0x8003D88C, 0x8003DE9C, 0x80082508, 0x80082728 and 0x80082870 through the framework decompiler. The old 0x800D1000 value remains a useful coarse fit and is not a resident address.

## cd

### RE-04 — CD load chokepoints and the loader's contract
- status: re-partial
- deps: RE-01
- evidence: C010/I009 locate the game-level synchronous path loader FUN_80082508(path,dest) and its inner contract FUN_80082608. Ghidra decompile plus exact calls identify stock CdSearchFile 0x80092AE8, CdRead 0x80093AF0 and CdReadSync 0x80093BF4. C012/I012 instruction-derive CdControl/CdCommand 0x80091DE4(cmd,param,result,async), pre-command/CdSync 0x80091898 and the common interrupt-result service 0x80091310. The service maps INT1 to ready state 0x800A0AE5/result 0x800A1978/callback 0x800A0808 and INT2/3/5 to sync state 0x800A0AE4/result 0x800A1970/callback 0x800A0804. Live mode-0xA0 ReadN derives LBA16 from Setloc, acknowledges INT3 then INT1, and DMA3 moves data, proving the command result is serviced. The same trace then delivers 21,164 contiguous sectors against a conservative physical upper bound of 3,451 in its 23-second watchdog denominator.
- where: tools/verify_cd_command.py; game/core/game_config.cpp (CD fields intentionally zero during the raw-controller A/B); framework CDC fix coordinated outside this repo
- gap: The generic following-sector timing defect is resolved and FMV's class is now proven by post-load execution, not destination alone. RE-04 remains partial because the located game loader still cannot be wired to psxport's incompatible `cdFileLoad` field: Toy Story 2 uses `(path,dest)`, while that seam expects `(dest,lba,size)`. The corrected route crosses RE-16 and remains CD-stable through field 10,303; this does not make the incompatible optional seam valid.
- notes: Reproduce the static/opposite CD evidence with `python3 tools/verify_cd_command.py --selftest`. The exact serviced phase sequence remains `16x1, 18x1, 12717x1, 12505x1, 12715x2, 12506x209, 316x1, 317x31, 317x31, 1746x1, 1748x79`; issue #9 owns that resolved timing defect. Reproduce FMV placement/class evidence with `python3 tools/overlay_map.py --check --selftest` and the direct headless port route. Do not special-case BIOS `A0:0x25` in this game.

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

### RE-10 — guest VBlank callback and host field delivery
- status: re-verified
- deps: RE-01
- evidence: C013 and I013. Exact Ghidra/decompile chain identifies graphics init 0x8003A650 registering VBlank handler 0x80039D60; that handler increments gp+0x7FC and routes deferred display work through 0x80021028, while 0x8003FA68 waits on both states. A live A/B over the same retail input changed a 30-second no-present watchdog in 0x8003FA68 into non-black legal/ESRB frames at presents 30, 120 and 900, then reached emitted FMV code.
- where: game/sync/field_clock.h; game/sync/field_clock.cpp; game/core/toystory2_runtime.cpp (installation only)
- gap: Complete for host field delivery: the game-local runtime arms the field clock only after the guest performs its exact graphics init, then samples pad, invokes the exact registered guest callback, advances SPU and presents once per GPU field. This does not identify RE-05's OT/packet-pool layout or assert gameplay rendering is complete.
- notes: `ToyStory2Runtime` owns field-clock installation through inheritance; the reusable clock mechanism stays in `game/sync/`, while Toy-specific callback identity remains out of shared psxport. The legacy config/hooks tables contain compatibility facts and fail-fast behavior only.

## assets

### RE-08 — the .RAW container: the RNC ProPack method byte and a working decompressor
- status: re-verified
- deps:
- evidence: THE METHOD-BYTE QUESTION DISSOLVED BY MEASUREMENT — see docs/info/claims/019 (superseding 005). The framing half of C005 survives (14-byte header: be32 unpackedLen, be32 packedLen, be16 unpackedCRC, be16 packedCRC, u8 leeway, u8 chunkCount; 0xFFFFFFFF sentinel), but the payloads are NOT RNC ProPack in any variant. Attempted decompression per C005's own falsifier, via temisu/ancient's algorithms ported to scratch harnesses: RNC1-new fails 39/39 LEVEL01 chunks, RNC2-new 38/39 (big-endian-word variant identical), RNC1-old/RNC2-old all fail on backward-copy range. The codec that passes is Traveller's Tales' OWN flag-bit LZ scheme — `DecompressRAW` per mateusfavarin/tsr's Ghidra research (MIT) — transcribed faithfully into tools/raw_unpack.py. RESULT: 46/46 .RAW files, **813/813 chunks**, each decoding to exactly unpackedLen with crc16-ARC matching BOTH header fields (be16@0x08 == unpacked CRC on 39/39 LEVEL01 chunks — C005's unchecked blind spot now verified). INDEPENDENT WITNESS: decoded LEVEL01 output byte-matches all 39 mouksx sub-file extractions of the SHA-1-identical level.raw.
- where: tools/raw_unpack.py (--selftest gates five classes incl. an unpacked-CRC-field negative); instrument docs/info/instruments/015
- gap: none for the container itself. NEXT LAYER NAMED: each decoded chunk begins with an LE u32 behaving like a command/packet ID (LEVEL01 chunks read 0x0..0x11 across its first ten), the shape of tsr's loader taxonomy (<0x20 texture packets etc.) — TS2's OWN command-ID table and packet structures are UNMEASURED and gate the asset pipeline beyond raw bytes.
- notes: TRAP recorded in C019: tiny LEVEL01#19 (35->64 B) ALSO decodes under RNC2-new with both CRCs passing AND matching the independent extraction — one chunk can "confirm" RNC2; only the corpus discriminates. The temisu/ancient decision in docs/references.md is updated by this outcome: ancient's decoders were the right thing to TRY and are now measured irrelevant to this container (its SHAPE was used for the falsification harnesses only; nothing vendored). Reproduce with `python3 tools/raw_unpack.py --all` and `python3 tools/raw_unpack.py --selftest`.

### RE-15 — the decoded .RAW packet layer: TS2's command-ID table and per-packet structures
- status: todo
- deps: RE-08
- evidence: RE-08's decoder delivers 813 verified chunk streams; each decoded chunk begins with an LE u32 behaving like a command/packet ID — LEVEL01 chunks 0..9 read 0x0,0x1,0x2,0x3,0x4,0x5,0x8,0xE,0x10,0x11 (`scratch/raw/LEVEL01__LEVEL.unpacked.bin`). tsr's `File_LoadRAW` research gives the SHAPE of such a table for Toy Story Racer (<0x20 texture packets, 0x20..0x26 framebuffer packets, pointer-table ranges, 0x101..0x103 cache packets) but that is ANOTHER GAME'S mapping.
- where: no code yet; evidence source is the guest loader code in SLUS_008.93 via Ghidra (the consumer of these chunks in-engine), cross-checked against our decoded bytes
- gap: NOTHING ABOUT TS2'S OWN TABLE IS MEASURED. Do not import TSR's command meanings by ID equality; derive them from this executable's loader and confirm each against our decoded packets before any struct reaches code.
- notes: The natural instrument is a packet-census tool beside tools/raw_unpack.py that prints per-ID counts/sizes with denominators and refuses unknown IDs loudly rather than skipping them.

### RE-09 — the scene / collision / model formats: .DAT, TERRAIN.ALL, .ANM
- status: todo
- deps: RE-00, RE-08
- evidence: A STARTING HYPOTHESIS ONLY, and it is other people's, not ours — docs/references.md. The strongest piece is a field-exact structural agreement between two independent REs of two different TT games: juanmv94 (from TS2 savestates) and mateusfavarin/tsr's docs/DAT.MD (from decompiled Toy Story Racer loaders) describe the same 0x20-byte object transform, offset for offset (s32 posX/Y/Z at 0x00, s16 rotX/Y/Z at 0x0C, s16 scaleX/Y/Z at 0x12, u32 mesh-stream offset at 0x1C), plus shared conventions: Euler rotation in PSX angle units, fixed-point scale with 0x1000 == 1.0, and DAT positions stored in HALF world units.
- where: no code yet
- gap: NOTHING HERE IS MEASURED AGAINST OUR BYTES. Two independent people agreeing is strong evidence about the ENGINE FAMILY and zero evidence about SLUS_008.93 specifically, and the sources carry documented per-game deltas (TS2 collision coordinates are s16 relative to the first point where Rascal used s32 absolute; TS2 vertices are s16 xyz + one rgb15 u16 = 8 bytes where A Bug's Life used 12 with rgb24; TS2 chains further 0x0100 blocks per collision object where Rascal and Bug's Life do not). There is also a build delta INSIDE this game: the TS2 demo's object struct is 4 bytes smaller than the retail one, so pin the retail USA build and never mix notes across builds. Confirm each field against our extracted files before any of it reaches code.
- notes: juanmv94's TravellersTalesPSXCollisionViewer is the best PSX-side document in existence for this engine and is UNLICENSED (all rights reserved) — read and learn only, never copy; it additionally embeds a lifted proprietary x86 blob and 54 copyrighted RAM images. Its own sample file corresponds to our LEVEL05/LEVEL1.RAW, i.e. it is PSX-targeted at our exact file layout, which is what makes it worth reading at all.
