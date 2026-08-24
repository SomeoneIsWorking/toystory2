---
id: C020
kind: claim
status: holds
created: 2026-08-24
tags: recomp,overlays,re14
depends: game/recomp_seeds.json#overlay_seeds, tools/verify_frame_fence.py#classify_post_fix
---

## Claim

RE-14's unclassified LEVEL01 target `0x800D12C4` is a TRUE OVERLAY FUNCTION ENTRY — the entry of every module loaded at slot base `0x800D12C0` — not an internal/coroutine re-entry and not a corrupted pointer; seeding it as `LEVEL01__LEVEL`'s overlay entry advances the live route past the former fail-fast

## Evidence

MEASURED 2026-08-24, four independent signals plus the landing itself. (1) MODULE HEADER LAYOUT: all 21 scannable LEVEL modules begin with one small sequential ID word — LEVEL01..LEVEL10 → 4..13, LEVEL1 variants → 0x14..0x22 (LEVEL00's file is the 4-byte stub) — so +4 is where module CONTENT starts by construction. (2) PROLOGUE AT +4: LEVEL01/LEVEL.BIN offset +4 is a genuine MIPS function entry (`27BDFFE8` addiu sp,sp,-0x18; `AFBF0014` sw ra,0x14(sp); `AFB00010` sw s0,0x10(sp); `8C900000` lw s0,0(a0)); other modules open at +4 with real code too (`lui v0,0x800D` sequences, `jr ra` stubs for the 24-byte files). (3) NOT CORRUPTED: the miss-time RAM dump holds the loaded file byte-identical at 0x800D12C0 for 13,336 of its 13,868 bytes (tail = region beyond the verified prefix, unchanged conclusion). (4) THE GAME ITSELF CALLS IT: boot text carries a DIRECT `jal 0x800D12C4` guarded by a state compare (emitted shard_3.c L_80029FB0 path: load word at 0x800A16B8, compare against 7, then jal), AND five 0x98-stride RAM descriptors (0x800C30F8..0x800C3358) each store 0x800D12C4 as their entry value. A coroutine/mid-function re-entry would sit at a body-interior resume address, not image-start-plus-one-header-word, and would not be a static direct-jal target. CLASS REJECTION IS WHY THE SEED LANDED ONLY NOW: per the frontier rule, no seed changed until the other two classes were rejected on evidence like this. LANDING: seed `"LEVEL01__LEVEL": ["0x800D12C4"]` emits `ov_level01__level_func_800D12C4` and advances the whole-module denominator to 22 modules / 309 functions. The live headless route goes from fail-fast `[recomp-MISS] 0x800D12C4` at 2,980 commits to executing the entry and reaching a NEW terminal boundary — `[mem:error] FATAL: UNMAPPED RAM read16 @ 0xEDA4F893` via func_800426E0←8002518C←80025E44←8002A070←8007B254 — at **8,320 commits** with max captured field 3,096 under cap 65,536; title frames measured at presents 1500/2100 (blue≈560k px of 691k, yellow/red/green families present), transition/black controls at 900/2400. Both gate tools require the moved boundary's exact address and classified model consumer (C021) and pass on the fresh trace (`scratch/logs/frame-fence-final.log`); the Clang build and complete 11/11 CTest gate pass.

## What would falsify it

a Ghidra read of SLUS_008.93 showing the direct `jal` target is formed differently than emitted (e.g. the emitter mis-decoded the call site); a second live run where the same dispatch reaches a DIFFERENT address while LEVEL01 is resident (would mean the descriptor value is data-dependent, i.e. computed, not a fixed entry); or discovery that the five RAM descriptors are consumed as data rather than as call targets
