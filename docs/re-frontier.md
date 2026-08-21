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

## THE STATE OF THIS PORT, 2026-08-21: a measured substrate reaches the CD-completion boundary

**RE-00 supplies Ghidra and RE-01 proves the complete crt0 group.** `tools/verify_crt0.py` resolves the
entry decompiler's bad-data truncation from the instruction stream and gates every shipped boot field.
RE-03 proves the two resident overlay slots and wires every known code module's load base. RE-02 now
emits the executable plus all 21 measured modules, links the real port, and matches the independent CPU
oracle at the executable's first call in all 34 state fields. The first live dispatch miss identified
the IRQ resume at `0x80088A2C`; its binary shape proves it is a mid-function re-entry, and the landed
emitter now discovers and dispatches that seed class directly. Boot advances through interrupt
registration, disc opening and serviced stock-libcd results. RE-04 now instruction-verifies
`0x80091DE4(cmd,param,result,async)`, its sync loop `0x80091898` and interrupt-result service
`0x80091310`. The honest boundary was one layer lower: psxport's CDC made the following-sector INT1
available immediately rather than at the mode-selected drive rate. Pinned psxport `3418a79b` removes
that race without game HLE: both direct and default routes advance through eleven real ReadN phases
into the emitted BITS/MEMORY overlay path. RE-02 remains partial because no later computed-entry miss
has surfaced yet.

**What IS otherwise measured is the DISC, not a game implementation.** The executable's identity, the
existence and byte count of 21 code overlays, the old coarse fitted base, the PSY-Q cohort, and the
`.RAW` container's framing — all in `docs/info/claims/`, each with its falsifier. C009 fills the crt0
group and C010 fills the overlay map; the remaining disc measurements do not fill shipping fields.

**RE-00 now stands up Ghidra on `SLUS_008.93`.** This game still has **no
decomp** (`docs/references.md`) — no symbol map, no function boundaries, no matching build — so the
decompiler is not a convenience here; it is the *entire* RE supply. With RE-00 verified, that supply no
longer blocks the first code RE. The sibling ports could locate a value in a reference and then confirm
it; this port must find it first. Budget accordingly: this is a materially harder starting position than `vagrant` (CC0
`rood-reverse`, ~62% matched) or `megamanx4` (AGPL `sozud/mmx4`, byte-identical target).

The next concrete target is to instruction-classify the BITS/MEMORY overlay path at
`0x800D9704`/`0x800D95C4` through resident caller `0x8003FA68`.
Any later `[recomp-MISS]` continues to grow RE-02 empirically.

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
- gap: Complete. The crt0 group is not the substrate; RE-02 is now ready because RE-03 has supplied the 21 streamed modules' verified resident bases.
- notes: Reproduce the portable gate with python3 tools/verify_crt0.py --check and python3 tools/verify_crt0.py --selftest. In the shared workspace, add --cross ../Tomba2Engine/scratch/bin/tomba2/MAIN.EXE for the genuine second-binary negative. The verifier resolves Ghidra truncation by proving break 0x80082E08 ends control before the referenced inline word 0x80082E10.

### RE-02 — recompiler seed set for SLUS_008.93
- status: re-partial
- deps: RE-01, RE-03
- evidence: C011, I010 and I011. tools/recomp_substrate.py identity-checks SLUS_008.93, re-runs the RE-01/RE-03 shipping gates, and drives the shipping emitter over 245,148 bytes: 321 binary-rooted resident seeds -> 864 functions and 176 roots -> 243 functions across exactly 21 overlay modules / 72 generated TUs. tools/compare_recomp_boundary.py then executes generated crt0 and the independent Mednafen CPU oracle to the first actual jal 0x80089344: 34/34 state fields agree; a forced a0 mutation produces one named mismatch. Default live boot surfaced [recomp-MISS] 0x80088A2C from irqPoll with guest RAM[0x8009ECD0] holding the target. Exact words and Ghidra show no prologue, live-v0 use and a shared stack epilogue, so it is recorded solely in main_reentry. Pinned psxport `3418a79b` makes that class a discovery root and emits its wrapper/body/dispatch directly; regeneration still declares and dispatches func_80088A2C without a duplicate main seed.
- where: tools/recomp_substrate.py; tools/compare_recomp_boundary.py; tests/toystory2_recomp_boundary.cpp; game/recomp_seeds.json; game/core/recomp_register.cpp; generated/ is gitignored
- gap: Seed completeness past the current boot boundary is unmeasured. With generic BIOS `A0:0x15` (`strcat`) present, the default run appends both measured `.vh`/`.vb` suffixes and reaches the instruction-verified stock-libcd command/result path. The old `692b9b20` baseline delivered 21,164 contiguous sectors in a 23-second denominator instead of at most 3,451. Pinned `3418a79b` yields the opposite answer on direct and default routes: 358 total sectors across eleven serviced ReadN phases (longest: 209 sectors from LBA12506), followed by the emitted BITS/MEMORY overlay path at `0x800D9704`/`0x800D95C4` through resident caller `0x8003FA68`, with no `[recomp-MISS]`. Continue adding only addresses surfaced by a real `[recomp-MISS]`, with entry-vs-reentry classification from exact bytes.
- notes: Never copy another game's seeds. A foreign address can split a real function at an arbitrary offset while emission still succeeds. Reproduce the committed evidence with `python3 tools/recomp_substrate.py --selftest`, build `toystory2_recomp_boundary`, then run `python3 tools/compare_recomp_boundary.py --selftest --oracle build/psxport_build/tools/oracle/oracle_trace --runner scratch/bin/toystory2_recomp_boundary`.

## overlays

### RE-03 — the overlay loader: how the load base is computed, and how many slots are resident
- status: re-verified
- deps: RE-00
- evidence: C010 and I009, built on C002/C003 rather than replacing their stated denominators. Ghidra xrefs locate all four level-name literals only in path builder FUN_80082870. Decompile and exact instruction checks show FUN_8003D88C selects one name, calls that builder once, then calls fixed-slot wrapper FUN_8003DE9C once; the wrapper forms a1=0x800D12C0 and calls file loader FUN_80082508. On a compatible path the same caller first loads `bits\memory.bin` to 0x800D5D20. The 19,040-byte interval equals the largest LEVEL module, while 5/10 LEVEL+LEVEL1 pairs exceed it, proving alternative contents of one slot. All 496 out-of-main jal targets were rescored at exact call-site destinations: 487 land in the level slot and zero in every other fixed destination; four modules score strictly better at 0x800D12C0 than at the old 4 KiB floor 0x800D1000, none worse. MEMORY.BIN is 63,312 bytes at [0x800D5D20,0x800E5470); its first 11 absolute address words map inside this placement, the loader preserves sector-rounded tail bytes, and the caller computes next frontier 0x800E54F8. `--selftest` passes 10/10, including an in-memory mutation of all four MEMORY/FMV destination pairs to 0x800D9D20 that forces the opposite `co-resident-possible` answer.
- where: tools/overlay_map.py; game/core/game_config.cpp (LEVEL and MEMORY overlaySlots); game/recomp_seeds.json (LEVEL pattern and BITS__MEMORY base)
- gap: Complete for the 21 measured code overlays. FMV/FMV.BIN shares 0x800D5D20 on other call paths but remains class-unresolved under RE-04 and is not emitted as code. A relocated module or a second level load on one invocation would falsify C010 and reopen this step.
- notes: Reproduce with `python3 tools/overlay_map.py --check` and `python3 tools/overlay_map.py --selftest`. Ghidra evidence: `python3 tools/re_xref.py --project ts2boot_re00 scratch/decomp/re03-xrefs.txt 80022F84..80022FAC 80021E08 800D12C0 800D5D20`; decompile 0x8003D88C, 0x8003DE9C, 0x80082508, 0x80082728 and 0x80082870 through the framework decompiler. The old 0x800D1000 value remains a useful coarse fit and is not a resident address.

## cd

### RE-04 — CD load chokepoints and the loader's contract
- status: re-partial
- deps: RE-01
- evidence: C010/I009 locate the game-level synchronous path loader FUN_80082508(path,dest) and its inner contract FUN_80082608. Ghidra decompile plus exact calls identify stock CdSearchFile 0x80092AE8, CdRead 0x80093AF0 and CdReadSync 0x80093BF4. C012/I012 instruction-derive CdControl/CdCommand 0x80091DE4(cmd,param,result,async), pre-command/CdSync 0x80091898 and the common interrupt-result service 0x80091310. The service maps INT1 to ready state 0x800A0AE5/result 0x800A1978/callback 0x800A0808 and INT2/3/5 to sync state 0x800A0AE4/result 0x800A1970/callback 0x800A0804. Live mode-0xA0 ReadN derives LBA16 from Setloc, acknowledges INT3 then INT1, and DMA3 moves data, proving the command result is serviced. The same trace then delivers 21,164 contiguous sectors against a conservative physical upper bound of 3,451 in its 23-second watchdog denominator.
- where: tools/verify_cd_command.py; game/core/game_config.cpp (CD fields intentionally zero during the raw-controller A/B); framework CDC fix coordinated outside this repo
- gap: The first missing service state was generic following-sector drive timing, not a Toy-specific callback or loader: old psxport `692b9b20` announced another INT1 from each BFRD request immediately. Pinned `3418a79b` schedules the first and following sectors in deterministic guest cycles (225,792 cycles at mode `0xA0`); direct and default routes retain stock interrupt acknowledgement and DMA, return first-INT1 status `0x22`, bound the longest phase to 209 sectors, and advance through eleven ReadN phases into the emitted MEMORY overlay. RE-04 remains partial at that later loader boundary. The located game loader still cannot be wired to psxport's `cdFileLoad`: that field expects `(dest,lba,size)`, while Toy Story 2's routine is `(path,dest)`. FMV/FMV.BIN remains class-unresolved despite sharing the MEMORY destination; destination alone does not make it code.
- notes: Reproduce the static/opposite evidence with `python3 tools/verify_cd_command.py --selftest`. Capture the landed answer with `PSXPORT_DEBUG=cdc,cdcw,cdcr,irq PSXPORT_NOAUDIO=1 PSXPORT_NOWINDOW=1 PSXPORT_WATCHDOG=3 PSXPORT_WATCHDOG_BOOT=20 ./run.sh`, then classify it with `python3 tools/verify_cd_command.py --trace <log> --expect bounded --expect-int1-status 0x22`. The exact serviced phase sequence is `16x1, 18x1, 12717x1, 12505x1, 12715x2, 12506x209, 316x1, 317x31, 317x31, 1746x1, 1748x79`. `scratch/logs/re04-cdc-3418-live.log` and `scratch/logs/re04-cdc-3418-default.log` are the local final captures; the old runaway baseline is `scratch/logs/re04-cdc-irq-live.log`. Resolved issue #9 owns the symptom and root cause. FMV/FMV.BIN (510,960 B) remains honestly unresolved: 3.2% code-plausible/no base fit leans data, but 68 `jr $ra` words prevent a pure-data claim.

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
