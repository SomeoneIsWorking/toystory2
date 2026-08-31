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
observe its counter or deferred-work flag advance. The now-retired game-local field clock invoked the exact
registered callback at the GPU rate. Live headless A/B changed zero presents plus a 30-second watchdog
into non-black legal/ESRB frames at presents 30, 120 and 900. Boot then loads FMV/FMV.BIN at
`0x800D5D20`, calls file+`0x908` (`0x800D6628`) and executes emitted FMV code. Pinned psxport
`d2266f4b` first carried A0:0x25 and generic lowering of the resident renderer's internal 32-way
computed jump table; current pin `54af32cb` retains both under the full Clang/static gate. The last
live retail trace completes its first path, both formerly missing renderer slots execute,
and RE-13 closed the then-title-owned frame fence: the field callback committed the shared captured queue
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
the live route observes slot 9 clear/reload and crosses the retired fault. RE-06 now derives the retail
pad buffers and driver contexts, wires the host field sample into them, and observes both forced Cross
and release packets live. RE-17 is now in progress: a bounded forced-input product run reaches Andy's
Room, pauses/unpauses and visibly moves the camera, but the unchanged recorded input diverges at the
pause transition when replayed. Issue #20 owns that remaining deterministic-replay defect.
RE-05's packet-pool parity and current pointer are now measured; OT extent and title-native producer
structures remain independently partial. Visible boot/title frames do not close either gap.

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
- evidence: C016, resolved issue #13 and scratch/decomp/re11-fmv-parser.c: retail FMV 0x800D8E48 calls executable Sony leaf 0x80082E5C; that leaf selects A0:0x25, and return 0x800D8E50 stores v0 as the normalized path byte. Current pin 54af32cb retains shared toupper and passes the complete Clang/static gate. The last bounded retail trace on d2266f4b observes the exact 15 normalized bytes of `toy2fmv\acti.str` plus 62 same-caller calls, then passes the renderer and frame fence to 0x800D12C4.
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
- where: historical `game/sync/field_clock.cpp` (retired by RE-18); tools/verify_frame_fence.py
- gap: Complete for the guest-loop frame fence and first title. A cap increase, primitive drop, return to raw gpu_present, per-field capture reaching capacity, or title discriminator accepting either real negative reopens it.
- notes: This remains the verified historical fence/root-cause result. RE-18 retires its callback-driven owner in favor of one finite title FrameDriver commit; the prior live trace does not certify the new loop.

### RE-18 — native-owned resident frame slice and fatal guest VSync
- status: re-partial
- deps: RE-01, RE-10, RE-13
- evidence: Exact generated instructions and Ghidra decomp identify main 0x8007A9E8's synchronous boot prefix, pre-resident switch, interactive-selection loop, resident two-field wait 0x8003FA68, deferred field service 0x80021028, normal update 0x8007B254, alternate update 0x8007B850, graphics initializers 0x8003A218 and 0x80039D9C, guest VBlank 0x80039D60, and linked libetc VSync 0x80088628 ending at helper 0x80088770 and returning counter 0x8009FD54. The title runtime creates the shared shell's required FrameDriver. The production sequencing boundary executes input -> one transition field or two resident fields -> deferred service -> one finite outer-loop operation -> audio -> one commit, mirrors fields to this title's measured counters, and selects either resident owner from 0x800A10F4. Boot returns before MEMORY initialization, then the first finite frame finishes it. Runtime overrides transcribe both graphics initializers' measured state without their VSync calls; generated bodies remain unchanged. The exact linked VSync body is fatal through the second narrow HLE window. Successive bounded launches classified stock-libcd VSync timeout queries, linked-libpad negotiation, libgpu timeout arm, a dropped selected-level ABI, and blocking pre-resident fade/graphics setup. Re20 crossed the corrected selected-level route, then hit 0x80039D9C through 0x8007C344; its only new presents were fully black. The current source transcribes one 0x8007C344 transition iteration per host frame and the 0x8007BEC4 tail rather than synchronously dispatching either blocking owner. Re21 (PID 2678458) crossed it with no guest VSync/fatal and reconciled all 300 capped frames; presents 45-300 visibly show coherent Andy's Room demo gameplay. Clang builds the seam and full product; 11/11 frame-driver/transition/pad/widescreen-policy tests (83 checks), 4/4 projection/GPU-timeout tests (29 checks), 3/3 stock-libcd tests (31 checks), and 19/19 frame-fence/static ownership classes pass.
- where: game/boot/guest_main_boot.*; game/boot/native_sync_overrides.*; game/loop/resident_frame.*; game/loop/resident_preparation.*; game/loop/outer_loop.*; game/loop/toystory2_frame_driver.*; game/core/toystory2_runtime.*; game/core/game_config.cpp; tests/toystory2_frame_driver_boundary.cpp; tests/toystory2_projection_boundary.cpp
- gap: Issue #25. Resident gameplay is runtime-verified; independent MEMORY/FMV loop owners remain unmigrated. The tracked VSync census currently contains 44 resident, 11 MEMORY, and one FMV direct call, with all other overlays at zero. This is not a complete end-to-end loop.
- notes: The old callback-driven graphics-init override, host turn, and guest-VBlank dispatch are deleted. The new graphics-init replacements own state initialization only and neither advances time nor presents. Toy Story 2 now declares `widescreenOnly()` rather than falsely exposing Native/lerp before their producers exist. Its neutral commit intentionally does not invoke `Fps60`: without title temporal state that would duplicate frames, not lerp motion. Do not restore successful VSync timing to make a caller pass; migrate each live owner and retain the fatal VSync assertion. The operation split follows Dusklight's ownership pattern: the product shell iterates, the title owns finite simulation/service order, and shared presentation remains a separate subsystem.

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
- notes: RE-16 is complete without a pointer substitution, skipped access, generated-C edit, or title/address special case. RE-06 subsequently completed pad delivery; RE-17 owns the still-missing bounded interactive gameplay witness. RE-05 subsequently measured the resident packet-pool parity and pre-GTE submission frontier; it remains partial independently.

### RE-17 — first bounded interactive gameplay witness
- status: in-progress
- deps: RE-06, RE-16
- evidence: Two bounded real-product runs at psxport 54af32cb, issue #20. The forced-input recording run reaches Andy's Room, shows the pause menu at present 4700, returns to gameplay at 5000, and held Right materially changes the camera view at 5400 and 6000; no watchdog, fatal or recomp-MISS occurs. The second run loads all 14,276 recorded uint16 samples unchanged and independently reaches Andy's Room plus the pause menu, proving downstream title response, but remains paused at presents 5000/5400/6000 instead of reproducing movement.
- where: scratch/logs/re17-drive-record.log; scratch/logs/re17-drive-replay.log; scratch/raw/re17-drive.pad; scratch/screenshots/re17_{record,replay}_{4700,5000,5400,6000}.png; docs/issues/0020-*
- gap: Real gameplay entry, pause response and visible movement are proven. Close RE-17 only after the exact pad recording deterministically reproduces the same pause/unpause and movement sequence; first compare delivered per-pad-frame masks and guest pause-state timing, then asynchronous CD/FMV scheduling if the masks agree. Retain the minimal failing replay in a redistributable tracked location once its stable reproducer is isolated.
- notes: Do not downgrade this to packet evidence: RE-06 proves delivery, while these captures prove downstream game response. Do not declare deterministic replay from the loader accepting the file; the visible record/replay split falsifies that. Do not use REPL before guest main returns; this title remains inside its guest-owned loop, so that command surface does not bound or drive boot.

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
- status: re-partial
- deps: RE-01
- evidence: Resident graphics init `0x80039D9C` initializes two complete render-buffer objects at `0x801BBD28` and `0x801DD21C`, exactly `0x214F4` bytes apart. The per-frame selector publishes chosen object+`0x2C4` through gp+`0x3E4`, whose resolved address is `0x800A10BC`; therefore the two packet pools begin at `0x801BBFEC` and `0x801DD4E0`. Wiring only those measured facts changes the live OT-attribution instrument from a named refusal to 15,521 recorded spans with zero overflow on a 70-frame real-disc run. It observes 64,139 guest-origin primitive submissions, attributing 18,705 and reporting 45,434 span misses. An interactive query at exact gameplay frame 59 sees roughly 2,060 drawing packet nodes and 17 directly attributed nodes, all below packet leaf `0x80084FCC`; the missing majority is therefore not evidence of an empty frame. The same live query independently counts the pre-GTE title submitters: `0x800100E4` 2,381 operations, `0x80017FF8` 801, `0x8001C920` 562, `0x8001DFE4` 383, `0x80014D3C` 128, `0x80083F14` 112, `0x80084024` 152, `0x80016690` 20, `0x800840A4` 6 and `0x8001A4D4` 2. Ghidra xrefs place the four dominant non-library leaves beneath object/mesh owners `0x80024174`, `0x8002518C`, `0x8002622C` and `0x80026D34`, all orchestrated by resident scene root `0x8002A070`, which both resident update owners `0x8007B254` and `0x8007B850` call. Both owners first call `0x8002C848(0x800C1540)`: that structure holds signed 32-bit camera position at `+0/+4/+8` and three angle halfwords at `+12/+14/+16`. The producer publishes exact position to scratchpad `0x1F800374`, coarse position to `0x1F800384`, its derived rotation matrix to `0x1F800394`, and the angle triplet to `0x800A1618`. Scene root passes those authored inputs to culling owners `0x8001ECD4`/`0x80020074`, which build visible-object pointer lists at `0x800BB4D8` and `0x800C0AB0` before mesh submission.
- live: The title now captures the authored camera input after every resident update into per-Core previous/current history and interpolates its 12-bit rotations over the shortest wrap. `0x8002C848` precedes the later scene root but is not adjacent to it; other update calls intervene. Bounded real-disc re37 traces changing samples from `(194774,34831,-399753)/(39,0,0)` to `(197184,44313,-350379)/(37,487,0)`, exits at 120/120 reconciled frames with zero dropped layers, and retains coherent Andy's Room captures without guest VSync/fatal.
- source slice: Dual-method xrefs over 148,992 resident words confirm visibility lists `0x800BB4D8`/`0x800C0AB0`, while decompile establishes that their submitted counts stay stack-local and differ from filtered residency globals `0x800A146C`/`0x800A14F4`. Common owner `0x8002622C` reads visibility record `+0x10` to the object record, selects active resource `+0x18+4*sceneBank`, reaches instance `+0x14`, and passes type-dependent mesh `+0x14`/`+0x1C` to dominant leaf `0x800100E4`. `ResidentSceneHistory` wraps both exact entries, captures bounded previous/current candidate batches plus actual mesh arguments/header words, then super-calls the unchanged generated bodies. Diagnostic real-disc re39 first reaches the wrappers on 89 resident updates: four batches per update, changing candidate totals 142..198 and mesh-call totals 66..86. The subsequent clean Clang build against landed psxport `3c342ec3` passes 20/20 CTests (including focused 13/13 scene boundary with 110 checks and full C++ policy). Its exact-product re40 rerun (PID 3160059) reproduces those live ranges, exits at 120/120 reconciled frames with zero dropped layers, and has no guest VSync/fatal/watchdog timeout. Inspected frames 60/120 remain coherent and distinct; a pixel-change instrument validated first on the identical-frame control reports 671,652/691,200 changed pixels between them. Binary-derived `resident_mesh_format.*` now decodes the signed header, eight-byte signed XYZ/colour vertices, positive versus non-positive command offsets, opcode-specific 12-/4-byte descriptor strides, packed byte indices, material/blend command bits, and 24..31 terminators before any GTE or packet output. Its bounded command walker records resident opcode/material/blend/primitive summaries; 12-byte attribute words remain raw rather than guessed as UV/material fields. This grounds live producer-input reachability plus the first source-format slice, not a native-rendered layer.
- where: game/core/game_config.cpp (packetPoolBase/Stride, poolPtrCur); game/core/toystory2_context.*; game/render/resident_camera_history.*; game/render/resident_scene_history.*; game/render/resident_mesh_format.*
- gap: OT base/extent, previous-pool pointer, clear/submit leaves, and the complete packet-writer attribution remain unmeasured. More importantly, packet ownership is not the native-render ownership seam: camera, exact visible-list entry arguments, common instance fields, dominant mesh entry arguments, and the base mesh command layout are now grounded, but an authorized live opcode/material histogram, exact 12-byte texture/material payload semantics, and separate 2D submitters remain open under issue #30. Native rendering, true widescreen and lerp remain missing until decoded authored inputs feed a visible title-native producer; retained history and a source decoder alone are not that producer.
- notes: Reproduce packet evidence with the real product under `PSXPORT_DEBUG=otattr`, then query `otattr` after `run 60`; `scratch/logs/re25-otattr-repl.log` is the retained exact-frame witness. Reproduce camera history with `PSXPORT_DEBUG=ts2-camera`; `scratch/logs/re37-current-camera-live.log` and `scratch/screenshots/re37-current-camera/contact-sheet.png` retain that witness. Clean re40 visibility/mesh evidence is in `scratch/logs/re40-scene-observation-clean.log` and `scratch/screenshots/re40-scene-observation-clean/`; re39 is its diagnostic precursor. Static callers are retained in `scratch/decomp/re27-producer-callers.txt`, `re29-producer-owner-callers.txt`, `re31-scene-orchestrator-callers.txt`, and dual-method visibility evidence in `re38-visibility-xrefs.txt`. Do not turn the non-empty packet pool into a post-GTE native path: replaying OT/GP0 output cannot widen title visibility or provide interpolation inputs.

### RE-06 — pad driver buffers
- status: re-verified
- deps: RE-01
- evidence: tools/verify_pad_buffers.py derives the unique 0x8003EF20 call arguments 0x800CF8A0/0x800CF8C8, linked-driver pointer fields 0x800A3E98/0x800A3F88 at 0xF0 stride, and independent consumer 0x8003AC58 reading slot 0. The shipping comparison matches all five GameConfig bindings; selftest passes 5/5 opposite/refusal classes. A bounded retail headless force-pulse writes active-low Cross byte 0xBF for fields 1..8, releases to 0xFF for 9..32, and pulses 0xBF again at 33, proving the host path can produce both packet answers at the measured buffer.
- where: tools/verify_pad_buffers.py; game/core/game_config.cpp; game/sync/field_clock.cpp; scratch/decomp/re06-pad-init.c, re06-pad-read.c, re06-libpad-chain.c
- gap: Complete for pad-buffer discovery and host packet delivery. This does not prove the title reacts correctly through menus into controllable gameplay; RE-17 owns that separate product witness.
- notes: padDriverFn stays zero because psxport does not read it. serviceFrame consults the measured 0x800A3E98 pointer field with 0xF0 stride, then falls back to the same fixed buffers before guest registration. No SIO function was overridden.

### RE-07 — platform HLE windows (the hardware-sync primitives)
- status: re-partial
- deps: RE-01
- evidence: C023, I019 and resolved issue #21. Identity-checked SLUS_008.93 proves SetGeomOffset 0x80083CD4 writes CR24/CR25 after shifting a0/a1, SetGeomScreen 0x80083CF4 writes CR26, and graphics init 0x8003A650 calls them with OFX=256, OFY=120, H=160. tools/verify_projection_publication.py matches the exact bodies, calls, four shipping constants and bindings with 6/6 positive/mutation/refusal classes. The hermetic title boundary installs exactly two handlers and passes 3/3 tests / 19 checks for guest GPR/GTE effects and same-Core ProjParams validity.
- where: tools/verify_projection_publication.py; tests/toystory2_projection_boundary.cpp; game/core/game_config.cpp (.hle projection pair only)
- live: Exact-`dbdb2baf` product tracing reaches both installed leaves four times, first at field 1 with the measured `256/120/160` arguments, and reports zero ABI violations.
- gap: Projection publication is now statically, differentially, and live-reach grounded. Every unrelated platform-sync address remains zero and unlocated; RE-07 is therefore partial, not a complete HLE map. This milestone does not enable widescreen, interpolate guest primitives, or create a native render producer.
- notes: The accepted half-open window [0x80083CD4,0x80083D00) covers only the two measured leaf bodies plus alignment padding. Do not widen it to an inferred Sony-library range. Next native-rendering work may consume the recorded authored projection only after its title producer/culling boundary is measured; interpolation belongs on authored camera/object transforms, never final guest GP0 primitives.

### RE-10 — guest VBlank callback and host field delivery
- status: re-verified
- deps: RE-01
- evidence: C013 and I013. Exact Ghidra/decompile chain identifies graphics init 0x8003A650 registering VBlank handler 0x80039D60; that handler increments gp+0x7FC and routes deferred display work through 0x80021028, while 0x8003FA68 waits on both states. A live A/B over the same retail input changed a 30-second no-present watchdog in 0x8003FA68 into non-black legal/ESRB frames at presents 30, 120 and 900, then reached emitted FMV code.
- where: historical `game/sync/field_clock.*` (retired by RE-18); game/core/toystory2_runtime.cpp
- gap: Complete for historical host field delivery. RE-05 now identifies the packet-pool parity and current pointer but not the full OT extent or title-native producer state; neither RE step by itself asserts native rendering is complete.
- notes: This is the verified historical cause and A/B witness. RE-18 retires the callback-driven implementation: `game/sync/field_clock.*` is deleted, no host turn dispatches guest VBlank, and the title FrameDriver owns fields directly.

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
- evidence: RE-08's decoder delivers 813 verified chunk streams; each decoded chunk begins with an LE u32 behaving like a command/packet ID — LEVEL01 chunks 0..9 read 0x0,0x1,0x2,0x3,0x4,0x5,0x8,0xE,0x10,0x11 (`scratch/raw/LEVEL01__LEVEL.unpacked.bin`). The first whole-retail census re-derived the complete non-FMV corpus from the configured disc and accepted 46 `.RAW` files / 813 chunks / 23,904,134 decoded bytes, yielding 52 distinct raw header values (`0x00000000` through observed `0x00000104`, non-contiguous). This proves only the header denominator, not an ID meaning. tsr's `File_LoadRAW` research gives the SHAPE of such a table for Toy Story Racer (<0x20 texture packets, 0x20..0x26 framebuffer packets, pointer-table ranges, 0x101..0x103 cache packets) but that is ANOTHER GAME'S mapping.
- where: `tools/raw_packet_census.py` consumes only `tools/raw_unpack.py`'s fully CRC-verified decoded chunks and reports raw little-endian u32 header values, chunk/byte denominators, and each ID's size range. It deliberately refuses a decoded chunk shorter than the proposed header and assigns no command semantics. The guest loader code in SLUS_008.93 remains the evidence source for every eventual packet meaning.
- gap: NOTHING ABOUT TS2'S OWN TABLE IS MEASURED. Do not import TSR's command meanings by ID equality; derive them from this executable's loader and confirm each against our decoded packets before any struct reaches code.
- notes: The natural instrument is a packet-census tool beside tools/raw_unpack.py that prints per-ID counts/sizes with denominators and refuses unknown IDs loudly rather than skipping them.

### RE-09 — the scene / collision / model formats: .DAT, TERRAIN.ALL, .ANM
- status: todo
- deps: RE-00, RE-08
- evidence: A STARTING HYPOTHESIS ONLY, and it is other people's, not ours — docs/references.md. The strongest piece is a field-exact structural agreement between two independent REs of two different TT games: juanmv94 (from TS2 savestates) and mateusfavarin/tsr's docs/DAT.MD (from decompiled Toy Story Racer loaders) describe the same 0x20-byte object transform, offset for offset (s32 posX/Y/Z at 0x00, s16 rotX/Y/Z at 0x0C, s16 scaleX/Y/Z at 0x12, u32 mesh-stream offset at 0x1C), plus shared conventions: Euler rotation in PSX angle units, fixed-point scale with 0x1000 == 1.0, and DAT positions stored in HALF world units.
- where: no code yet
- gap: NOTHING HERE IS MEASURED AGAINST OUR BYTES. Two independent people agreeing is strong evidence about the ENGINE FAMILY and zero evidence about SLUS_008.93 specifically, and the sources carry documented per-game deltas (TS2 collision coordinates are s16 relative to the first point where Rascal used s32 absolute; TS2 vertices are s16 xyz + one rgb15 u16 = 8 bytes where A Bug's Life used 12 with rgb24; TS2 chains further 0x0100 blocks per collision object where Rascal and Bug's Life do not). There is also a build delta INSIDE this game: the TS2 demo's object struct is 4 bytes smaller than the retail one, so pin the retail USA build and never mix notes across builds. Confirm each field against our extracted files before any of it reaches code.
- notes: juanmv94's TravellersTalesPSXCollisionViewer is the best PSX-side document in existence for this engine and is UNLICENSED (all rights reserved) — read and learn only, never copy; it additionally embeds a lifted proprietary x86 blob and 54 copyrighted RAM images. Its own sample file corresponds to our LEVEL05/LEVEL1.RAW, i.e. it is PSX-targeted at our exact file layout, which is what makes it worth reading at all.
