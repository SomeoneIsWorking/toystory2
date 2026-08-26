// game_config.cpp — measured Toy Story 2 (SLUS_008.93, USA) compatibility facts.
//
// ToyStory2Runtime is the title's ownership seam. This legacy GameConfig remains only because
// generic psxport algorithms still read `c->cfg->field`; each typed framework extraction must
// delete its corresponding fields here rather than growing this bag.
//
// READ THIS BEFORE FILLING ANYTHING IN.
//
// Only complete measured groups are filled (currently RE-01 crt0, RE-02's physical resident range,
// RE-03 overlay slots and RE-06 pad routing). Every un-RE'd guest address is still zero deliberately:
// psxport fails fast on a zero it needs, whereas a plausible-looking WRONG address does not fail
// cleanly — it breaks boot or diverges the byte-compare in a way that reads as a framework bug. Each
// group names its frontier step in docs/re-frontier.md.
//
// AND UNLIKE THE SIBLING PORTS, THERE IS NO SUPPLY TO BORROW FROM. vagrant has
// a CC0 matching decomp of its executable and megamanx4 an AGPL one whose
// declared build target is byte-identical to ours; both therefore have a
// temptation to resist and a rule ("a borrowed address is a hypothesis") aimed
// at it.
// **There is no decomp of Toy Story 2** (docs/references.md) — no symbol map,
// no function boundaries, no matching build. Every address that appears here
// must come from reproducible binary evidence on SLUS_008.93 in this repo. When you
// fill one, gate it against the executable bytes and cite that verifier; here
// binary evidence is the only provenance a value can have.
#include "game_iface.h"
#include "legacy_game_interface.h"

#ifdef TS2_HAVE_SUBSTRATE
#include "overlay_table.h"
#endif

// MEASURED, from the PS-EXE header of the extracted SLUS_008.93
// (tools/extract_exe.py prints it) and from the disc's SYSTEM.CNF. Kept as
// named constants rather than dropped into the struct below, because the
// struct's boot group is consumed AS A GROUP by the framework's crt0_setup: a
// lone entry PC beside a zeroed BSS range would make it run a wrong crt0
// instead of refusing.
//
//   sha1(SLUS_008.93) = f90c9cd6b4fc9845adfe34e306b7df393bf9154c   (598,016
//   bytes) PS-X EXE  pc0 = 0x80082D60   text = 0x80010000 + 0x91800   sp =
//   0x801FFFF0   gp0 = 0
//             d_addr/d_size and b_addr/b_size are all 0 in the header, so
//             t_size covers .data too and this game clears its own BSS in crt0.
//             File size 598,016 = 0x800 header + t_size exactly.
//   SYSTEM.CNF  BOOT = cdrom:\SLUS_008.93;1   TCB = 4   EVENT = 16   STACK =
//   801FFF00
//
// NOTE THE TWO STACK VALUES, because they look like a contradiction and one of
// them is going to matter: the header's s_addr is 0x801FFFF0 and SYSTEM.CNF's
// STACK is 0x801FFF00. SYSTEM.CNF wins at boot — the BIOS shell applies it
// before jumping to pc0. The verified crt0 overwrites both: it reads the inline
// word at 0x80082E10 (0x00200000) and ORs KSEG0 into it, producing
// sp=fp=0x80200000. Neither header/CNF stack value is the final guest stack.
static constexpr uint32_t kPsExeEntry = 0x80082D60u;     // header pc0
static constexpr uint32_t kPsExeTextAddr = 0x80010000u;  // header t_addr
static constexpr uint32_t kPsExeTextSize = 0x00091800u;  // header t_size (595,968 B)
static constexpr uint32_t kPsExeSpHeader = 0x801FFFF0u;  // header s_addr
static constexpr uint32_t kSystemCnfStack = 0x801FFF00u; // SYSTEM.CNF STACK=, which wins at boot
static_assert(kPsExeEntry >= kPsExeTextAddr && kPsExeEntry < kPsExeTextAddr + kPsExeTextSize,
              "the PS-EXE entry must lie inside the loaded text — if this fires, the "
              "header was "
              "misread and every number in this file's comment block is suspect");
static_assert(kSystemCnfStack < kPsExeSpHeader,
              "the two stack values are recorded because they DISAGREE "
              "(SYSTEM.CNF 0x801FFF00 vs header "
              "0x801FFFF0); if this ever fires, one of them was re-read and "
              "RE-01's note is stale");

// RE-01, measured from the verified executable by tools/verify_crt0.py. The
// verifier symbolically follows entry 0x80082D60 through the InitHeap return,
// second jal and terminating break. It prints every instruction chain and
// compares these constants back to this shipping file; --selftest mutates both
// sides, rejects malformed inputs and accepts a real second executable only as
// a cross-binary negative.
static constexpr uint32_t kCrt0BssZeroLo = 0x800A1070u;     // sw zero @ 0x80082D70
static constexpr uint32_t kCrt0BssZeroHi = 0x800D12C0u;     // sltu bound @ 0x80082D78
static constexpr uint32_t kCrt0StackTopBase = 0x80082E10u;  // lw v0 @ 0x80082DA4
static constexpr uint32_t kCrt0StackTopBase2 = 0x800A0764u; // lw v1 @ 0x80082DC4
static constexpr uint32_t kCrt0HeapBase = 0x800D12C0u;      // sll/srl mask @ 0x80082DB8
// A complete walk reaches InitHeap after exactly one absolute non-BSS store,
// `sw ra,4208(at)`. No store writes the computed heap size or base, so zero is
// the measured meaning ABSENT for these optional framework fields.
static constexpr uint32_t kCrt0HeapSizePtr = 0u;
static constexpr uint32_t kCrt0HeapBasePtr = 0u;
static constexpr uint32_t kCrt0Gp = 0x800A0CD8u;       // addiu gp @ 0x80082DE4
static constexpr uint32_t kCrt0LibcInit = 0x80089344u; // jal @ 0x80082DEC; A(39h) thunk
static constexpr uint32_t kCrt0GameMain = 0x8007A9E8u; // second jal @ 0x80082E00
static constexpr uint32_t kCrt0Entry = kPsExeEntry;
static constexpr int32_t kCrt0StackBias = 0; // no bias instruction between lw and or sp
static_assert(kCrt0BssZeroHi == kCrt0HeapBase, "this crt0 starts its heap exactly at the proven BSS upper bound");
static_assert(kCrt0StackTopBase >= kPsExeTextAddr && kCrt0StackTopBase < kPsExeTextAddr + kPsExeTextSize,
              "the stack-top word must be readable from the loaded executable image");

// ─────────────────────────────────────────────────────────────────────────────────────────────────────
// THE OVERLAY MAP, INSTRUCTION-VERIFIED BY tools/overlay_map.py --check.
// ─────────────────────────────────────────────────────────────────────────────────────────────────────
// FUN_8003D88C selects exactly one of level.bin/level1.bin/level2.bin/level3.bin,
// then calls the one fixed-destination wrapper FUN_8003DE9C. Its
// `lui a1,0x800D; addiu a1,0x12C0` passes 0x800D12C0 to file loader
// FUN_80082508. The same caller can first load BITS/MEMORY.BIN at 0x800D5D20.
// That next live slot makes the LEVEL window exactly 19,040 bytes, equal to
// the largest LEVEL module; five of ten LEVEL/LEVEL1 pairs exceed it, so they
// are alternative contents of one slot rather than simultaneous modules.
//
// MEMORY.BIN is 63,312 retail bytes at [0x800D5D20,0x800E5470). The loader
// preserves the sector-rounded tail and returns the exact CdlFILE size; the
// caller then computes its next arena pointer as
// `(size & 0x000FFFFC) + 0x800D5DA8 = 0x800E54F8`. Its first eleven absolute
// address words all map back inside that exact placement. `overlay_map.py`
// proves the call chain, compares these constants with both shipping consumers,
// and forces the opposite slot-count result by widening the next-slot bound.
static constexpr uint32_t kLevelOverlayBase = 0x800D12C0u;
static constexpr uint32_t kMemoryOverlayBase = 0x800D5D20u;
static_assert(kLevelOverlayBase == kCrt0HeapBase,
              "the level overlay slot starts at the independently verified crt0 heap base");
static_assert(kMemoryOverlayBase - kLevelOverlayBase == 19040u,
              "the next co-resident slot bounds the level overlay window");

// Physical resident range from the identity-checked PS-X EXE header. The recompilation gate checks
// these literals against generated/overlay_table.h after every emission; the build adds a second
// compile-time tripwire when the generated header is present.
static constexpr uint32_t kRecMainLo = 0x00010000u;
static constexpr uint32_t kRecMainHi = 0x000A1800u;
#ifdef TS2_HAVE_SUBSTRATE
static_assert(kRecMainLo == REC_MAIN_LO && kRecMainHi == REC_MAIN_HI,
              "GameConfig resident range disagrees with the emitted substrate");
#endif

// RE-06, measured from the identity-checked retail executable by tools/verify_pad_buffers.py.
// The game has exactly one call to its linked pad initializer at 0x8003EF20. Its two arguments are
// formed directly from these buffer addresses, and the initializer stores those pointers into two
// 0xF0-byte driver contexts:
//
//   0x8003EF10/14  lui/addiu a0 -> 0x800CF8A0
//   0x8003EF18/1C  lui/addiu a1 -> 0x800CF8C8
//   0x800972B0     sw s1,0x30(s0)  -> [0x800A3E98] = slot-0 buffer
//   0x800972B4     sw s2,0x120(s0) -> [0x800A3F88] = slot-1 buffer
//   0x800972FC     addiu a0,a0,0xF0 (next driver context)
//
// The game's input decoder at 0x8003AC58 independently forms 0x800CF8A0 and reads the standard pad
// packet there. The host field clock already calls Pad::serviceFrame before the guest VBlank handler;
// these facts supply the destinations that serviceFrame previously skipped because every pad field
// was zero.
static constexpr uint32_t kPadSlot0Buffer = 0x800CF8A0u;
static constexpr uint32_t kPadSlot1Buffer = 0x800CF8C8u;
static constexpr uint32_t kPadDriverPointerTable = 0x800A3E98u;
static constexpr uint32_t kPadDriverContextStride = 0xF0u;
static_assert(kPadDriverPointerTable + kPadDriverContextStride == 0x800A3F88u,
              "the measured per-port driver pointer fields must stay one context apart");

// DESIGNATED initialisers, deliberately. GameConfig is initialised POSITIONALLY
// by the older consumers in this workspace, and the framework appends fields to
// it — which means a positional list silently re-binds every value after an
// inserted field. Binding by name makes an upstream insert a no-op here and an
// upstream RENAME a compile error naming the field, which is the signal we
// want. C++20 requires designators in declaration order; keep them so when
// adding one.
static const GameConfig g_ts2_cfg = {
    // --- crt0 / boot ---------------------------------------- RE-01 re-verified --
    // This is one measured group, never a partial header-derived fill. Re-run
    // `python3 tools/verify_crt0.py --check` to diff every value against the
    // instruction stream that ships on the verified executable.
    .bssZeroLo = kCrt0BssZeroLo,
    .bssZeroHi = kCrt0BssZeroHi,
    .stackTopBase = kCrt0StackTopBase,
    .stackTopBase2 = kCrt0StackTopBase2,
    .heapBase = kCrt0HeapBase,
    .heapSizePtr = kCrt0HeapSizePtr,
    .heapBasePtr = kCrt0HeapBasePtr,
    .gp = kCrt0Gp,
    .libcInit = kCrt0LibcInit,
    .gameMain = kCrt0GameMain,
    .crt0 = kCrt0Entry,

    // --- recompiled MAIN .text range (physical) ---------------- RE-02 partial --
    // Header-derived literals, checked against generated/overlay_table.h by both the emitter gate
    // and a compile-time assertion whenever the substrate is present.
    .recMainLo = kRecMainLo,
    .recMainHi = kRecMainHi,

    // --- disc key ----------------------------------------------- this port's
    // own env name, not RE --
    // Not an RE fact but a port fact, and it belongs here because the framework
    // must not know it: the
    // resolver used to hardcode the FIRST consumer's variable, so a second port
    // set its own key,
    // nothing read it, and every boot ran with NO MEDIA behind an
    // ordinary-looking log.
    // tools/resolve_disc.py implements the same key on the host side, and
    // .env.example documents it.
    .discEnvVar = "PSXPORT_TS2_DISC",

    // --- boot intro movies -------------------------------------------
    // deliberately EMPTY, and why --
    // Not a gap. The framework's native .STR player only plays what a port ASKS
    // it to play, and this
    // port asks for nothing: there is no native boot here, so any movie is the
    // GUEST's to play on the
    // substrate. The disc does carry 22 .STR streams under TOY2FMV/ (DLOGO,
    // ACTI, TT, TRALER2, the
    // per-level intros, END01); naming one here without knowing which the boot
    // plays would be a guess
    // wearing a citation.
    .bootFmv = {nullptr, nullptr, nullptr, nullptr},

    // --- per-frame OT / packet pool
    // ---------------------------------------------- RE-05, NOT DONE --
    // ⬜ todo, NOT skip-by-design: unlike megamanx4 this port has made no scope
    // decision that removes
    // the native frame loop, and no measurement of this game's frame rate
    // exists in this repo. It is
    // simply not RE'd. Do not read the zeros as a decision.
    .otRegionBase = 0,
    .otRegionStride = 0,
    .packetPoolBase = 0,
    .packetPoolStride = 0,
    .otBasePtr = 0,
    .dwellCounter = 0,
    .poolPtrCur = 0,
    .poolPtrLast = 0,
    .clearOtagR = 0,
    .putDrawEnv = 0,
    .drawSync = 0,
    .irqEventClasses = {0, 0, 0},
    .dualviewRenderOrch = 0,
    .dualviewSubmit = 0,

    // --- scheduler task layout ------------------------------- N/A until a
    // native frame loop exists --
    // The framework's PcScheduler is not wired for this port: GameHooks'
    // scheduler entries are
    // fail-fast stubs, so these values would have no reader even if they were
    // known.
    .taskTableBase = 0,
    .taskSlotStride = 0,
    .taskCount = 0,
    .curTaskPtr = 0,
    .stageStart = 0,
    .stageDemo = 0,
    .stageGame = 0,

    // --- overlay router slots -------------------------------- RE-03 re-verified --
    // LEVEL{,1,2,3}.BIN are mutually exclusive contents of the first slot.
    // BITS/MEMORY.BIN occupies the second slot concurrently. FMV/FMV.BIN also
    // loads at the second address in other call paths, but its file class is
    // still unresolved under RE-04 and is therefore not represented as code.
    .overlaySlots = {{kLevelOverlayBase, "LEVEL"}, {kMemoryOverlayBase, "MEMORY"}, {0, nullptr}},

    // --- CD chokepoints ------------------------------------------- RE-04 partial --
    // RE-03 identifies the game-level synchronous path loader at 0x80082508 and
    // its stock-libcd chain (CdSearchFile 0x80092AE8, CdRead 0x80093AF0,
    // CdReadSync 0x80093BF4). RE-04 additionally proves CdControl 0x80091DE4,
    // CdSync 0x80091898 and the INT1..INT5 service state machine 0x80091310.
    // They remain zero because the verified shipping path uses the raw controller;
    // arming an HLE would bypass that path. Independently, psxport's
    // cdFileLoad field has the incompatible (dest,lba,size) ABI, while this
    // game's loader takes (path,dest); never register it under the wrong contract.
    .cdInit = 0,
    .cdCommand = 0,
    .cdSync = 0,
    .cdReadPrim = 0,
    .cdFileLoad = 0,
    .cdAsyncRead = 0,
    .voicePlay = 0,
    .voiceStop = 0,
    .lastSectorTracker = 0,
    .cdInlineLoad = 0,
    .cdCmdStream = 0,
    .cdCallbackTable = {0, 0, 0, 0},
    .cdCallbackFn = {0, 0, 0, 0},
    .cdGetSector = 0,
    .cdReadyCbPtr = 0,
    .cdLastPosBuf = 0,
    .cdReadStock = 0,
    .cdReadSync = 0,
    .cdSearchFile = 0,
    .dmaCallbackTable = 0,

    // --- pad driver --------------------------------------------- RE-06 re-verified --
    // The retail initializer takes the two fixed buffers directly and registers them in per-port
    // contexts. serviceFrame therefore consults the measured pointer fields first and falls back to
    // the same fixed buffers if the guest has not initialized the contexts yet. padDriverFn remains
    // zero because psxport never reads that retired compatibility field; no guest function is being
    // bypassed or falsely claimed as a host override.
    .padSlot0Buf = kPadSlot0Buffer,
    .padSlot1Buf = kPadSlot1Buffer,
    .padDriverFn = 0,
    .padSlotPtrTable = kPadDriverPointerTable,
    .padSlotPtrStride = kPadDriverContextStride,

    // --- platform HLE (the hardware-sync primitives)
    // ----------------------------- RE-07, NOT DONE --
    // ZERO MEANS "not RE'd, install nothing". initBuiltins() then registers no
    // handler and says so; a
    // run that needs one hangs in the guest's real spin loop, which is the
    // honest signal that the RE is
    // outstanding. The windows are zero too, so register_() refuses everything
    // — this game has not
    // stated its memory map yet, and a window guessed from another game's map
    // is how a handler lands on
    // an unrelated function.
    .hle = {},

    // --- adapter input: guest VRAM owns the picture throughout the verified route --
    // The renderer no longer reads this compatibility field. ToyStory2Runtime is legacy-backed,
    // so LegacyGameRuntimeAdapter projects it through the required per-Game
    // GameRuntime::guestVramIsPicture policy. The current port has no native producer: its measured
    // field loop invokes the guest VBlank callback and presents guest DrawOTag/VRAM output, including
    // upload-only screens. Therefore the one verified answer is true. If native picture ownership is
    // added later, migrate the title to a derived dynamic policy rather than changing this static
    // adapter input or adding a second copy of the same rule.
    .preserveVramBackdrop = 1,

    // --- memory card ------------------------------------------------------
    // this port's own key/path --
    // Worth knowing for later: BITS/MEMORY.BIN (63,312 B) is this game's
    // largest code overlay and is
    // almost certainly the memory-card module, by its path and by the
    // `bits\memory.bin` string in the
    // boot exe. That is a LEAD about where save handling lives, not a fact
    // about these two fields.
    .cardEnvVar = "PSXPORT_TS2_CARD",
    .cardDefaultPath = "scratch/saves/toystory2.mcr",

    // --- frame pacing ------------------------------------------- 1 field per
    // pacing call, and why --
    // The framework REQUIRES this field ("a new game MUST set this field"):
    // zero used to fall through to
    // reading the first consumer's engine byte out of the scratchpad, which in
    // any other game is
    // ordinary working memory, so a second consumer slept on garbage. THE
    // SEMANTICS ARE BY CALLING
    // CADENCE, NOT BY THE GAME'S DISPLAY RATE — a port that still runs the
    // guest's own frame loop and
    // paces once per FIELD sets 1, which is this port's shape (there is no
    // native loop here). Revisit
    // together with RE-05 if a native loop ever calls the pacer once per logic
    // frame.
    .paceQuota = 1,

    .windowTitle = "Toy Story 2 (psxport)",
    // Zero is a measured value, not an unset default. `declared = 1` tells
    // crt0_plan that tools/verify_crt0.py proved no bias instruction exists.
    .stackBias = {1, kCrt0StackBias},
};

const GameConfig &ts2::legacy::measuredConfig = g_ts2_cfg;
