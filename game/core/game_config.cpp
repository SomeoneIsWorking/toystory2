// game_config.cpp — the Toy Story 2 (SLUS_008.93, USA) GameConfig: the
// guest-address literals the PSX-generic framework reads through
// `c->cfg->field`.
//
// READ THIS BEFORE FILLING ANYTHING IN.
//
// **Every guest address in this file is ZERO, because none has been
// reverse-engineered in this repo.** Zero is the honest value and it is
// deliberate: psxport fails fast on a zero it needs, whereas a
// plausible-looking WRONG address does not fail cleanly — it breaks boot or
// diverges the byte-compare in a way that reads as a framework bug. Each group
// names the open step in docs/re-frontier.md.
//
// AND UNLIKE THE SIBLING PORTS, THERE IS NO SUPPLY TO BORROW FROM. vagrant has
// a CC0 matching decomp of its executable and megamanx4 an AGPL one whose
// declared build target is byte-identical to ours; both therefore have a
// temptation to resist and a rule ("a borrowed address is a hypothesis") aimed
// at it.
// **There is no decomp of Toy Story 2** (docs/references.md) — no symbol map,
// no function boundaries, no matching build. Every address that ever appears in
// this file will have come out of Ghidra on SLUS_008.93 in this repo. When you
// fill one, paste the disassembly line that justifies it, the way
// spider1/game/core/game_config.cpp does; that citation is what makes the value
// reviewable a year from now, and here it is the ONLY provenance a value can
// have.
#include "game_iface.h"

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
// before jumping to pc0. Which of the two (if either) this game's crt0 then
// overwrites is RE-01's job to measure; in the two sibling ports crt0 computed
// sp itself and ignored both. Do NOT resolve the discrepancy by picking one
// here.
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

// ─────────────────────────────────────────────────────────────────────────────────────────────────────
// THE OVERLAY WINDOW, MEASURED — and it is the one guest address in this file's
// comments that did NOT come from a header. It is deliberately NOT in the
// struct.
// ─────────────────────────────────────────────────────────────────────────────────────────────────────
// 2026-08-12: **Toy Story 2 streams R3000A code overlays** — 21 files, 245,148
// bytes = 29.1% of the game's code-bearing bytes (docs/info/claims/002-*). 15
// of them fit a load base of 0x800D1000 with 75-100% of their own out-of-.text
// jal targets inside [base, base+size) and a 0.0% runner-up, and the modules'
// own trailer tables hold absolute 0x800Dxxxx pointers
// (docs/info/claims/003-*). Reproduce with `python3 tools/base_fit.py
// --selftest`.
//
// WHY `overlaySlots` IS STILL ALL ZERO, and why filling it from that number
// would be the exact faked-step failure this repo must not accumulate:
//
//   1. HOW the loader arrives at 0x800D1000 is UNKNOWN. The constant appears
//   nowhere in the boot exe —
//      not as a lui/addiu pair (12,670 pairs folded over all 148,992 .text
//      words) and not as a literal word. It is presumably an end-of-BSS linker
//      symbol or a runtime allocation, which means the RESIDENT base could
//      differ from the fitted one and would still fit.
//   2. WHETHER LEVEL.BIN AND LEVEL1.BIN SHARE ONE SLOT OR OCCUPY TWO is
//   unresolved. Both fit
//      0x800D1000 — a contradiction if they are ever resident together — and
//      LEVEL06/LEVEL1.BIN's trailer points into LEVEL06/LEVEL.BIN's span, which
//      reads like two simultaneously resident modules. Both readings fit the
//      bytes. The slot COUNT is part of the answer, and this struct has exactly
//      three slots.
//
// An overlay is keyed BY its load address: a wrong base emits a whole module of
// correctly decoded instructions at wrong addresses, and every jal target,
// pointer test and router lookup then goes silently wrong — while the emit
// SUCCEEDS. So the base is a measurement waiting on RE-03's decompile of the
// function that references the overlay-name string group at VA
// 0x80022F84..0x80022FA8 (level1.bin at 0x80022F84, level.bin itself at
// 0x80022FA8), not a value to ship.
static constexpr uint32_t kFittedOverlayBase = 0x800D1000u; // MEASURED fit; NOT a resident base
static_assert(kFittedOverlayBase > kPsExeTextAddr + kPsExeTextSize,
              "an overlay slot must sit above the loaded image; if this fires, "
              "either the fit or the "
              "header was re-read and docs/info/claims/003-* is the arbiter");

// DESIGNATED initialisers, deliberately. GameConfig is initialised POSITIONALLY
// by the older consumers in this workspace, and the framework appends fields to
// it — which means a positional list silently re-binds every value after an
// inserted field. Binding by name makes an upstream insert a no-op here and an
// upstream RENAME a compile error naming the field, which is the signal we
// want. C++20 requires designators in declaration order; keep them so when
// adding one.
static const GameConfig g_ts2_cfg = {
    // --- crt0 / boot
    // ------------------------------------------------------------- RE-01, NOT
    // DONE --
    // The header facts above are NOT this group. crt0_setup consumes these
    // eleven fields together and
    // derives sp, gp, the BSS clear and the heap from them; writing the two or
    // three that a header
    // hands you and zeroing the rest is how a port runs a wrong crt0 instead of
    // refusing to run one.
    // Nobody has executed or decompiled this game's entry function at
    // 0x80082D60.
    .bssZeroLo = 0,
    .bssZeroHi = 0,
    .stackTopBase = 0,
    .stackTopBase2 = 0,
    .heapBase = 0,
    .heapSizePtr = 0,
    .heapBasePtr = 0,
    .gp = 0,
    .libcInit = 0,
    .gameMain = 0,
    .crt0 = 0,

    // --- recompiled MAIN .text range (physical)
    // ---------------------------------- RE-02, NOT DONE --
    // These come from the RECOMPILER's own generated/overlay_table.h
    // (REC_MAIN_LO / REC_MAIN_HI) so
    // they can never drift from the substrate they describe. There is no
    // substrate yet, so they are
    // zero and this file does not #include that header — including a generated
    // header that does not
    // exist would make the tree un-configurable rather than honestly
    // incomplete.
    .recMainLo = 0,
    .recMainHi = 0,

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

    // --- overlay router slots
    // ---------------------------------------------------- RE-03, NOT DONE --
    // THIS GAME HAS 22 CODE OVERLAYS and this is the field they will eventually
    // be routed through. It
    // stays zero for the two reasons spelled out in the block above
    // kFittedOverlayBase: the loader's
    // own computation of the base is not understood, and the number of
    // simultaneously resident slots is
    // not established. Zero here means the router has no slot, which is what a
    // port with no recompiled
    // overlays should say — and emit.py treats a missing base as a HARD ERROR,
    // deliberately, so the
    // refusal is the feature.
    .overlaySlots = {{0, nullptr}, {0, nullptr}, {0, nullptr}},

    // --- CD chokepoints
    // ---------------------------------------------------------- RE-04, NOT
    // DONE --
    // Load-bearing here in a way it is not in an overlay-free port: the CD path
    // is also the OVERLAY
    // LOADER's path, so RE-03 and RE-04 will be worked together.
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

    // --- pad driver
    // -------------------------------------------------------------- RE-06, NOT
    // DONE --
    // Nothing located. The PSY-Q cohort (sys.c 1.129 / intr.c 1.76 /
    // bios.c 1.86) says libpad rather
    // than a custom SIO driver is the likely SHAPE, which says what to look
    // for, not where.
    .padSlot0Buf = 0,
    .padSlot1Buf = 0,
    .padDriverFn = 0,
    .padSlotPtrTable = 0,
    .padSlotPtrStride = 0,

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

    // --- rendering policy ------------------------------------------------- 1
    // while the guest draws --
    // The renderer clears to black on the principle "show ONLY what a native
    // producer submitted". That
    // is right for a port whose native renderer owns the frame and WRONG for
    // one still running the
    // guest's drawing code, where an upload into the display area IS visible on
    // real hardware: logo
    // screens, FMV stills and menus that are uploads with no primitives render
    // black. This port owns no
    // drawing at all, so the guest's uploads must survive.
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
    // crt0 stack-top bias, MEASURED by psxport tools/crt0_extract over this
    // game's own boot
    // executable (SLUS_008.93, entry 0x80082D60). `declared = 1` is mandatory:
    // crt0_plan REFUSES a boot when it is 0,
    // because this game's measured bias IS 0 — it has no `addi v0,v0,N` at all,
    // like Mega Man X4.
    .stackBias = {1, 0},
};

const GameConfig *ts2_game_config() {
  return &g_ts2_cfg;
}

// Installs BOTH halves of the seam, because a Core's ctor snapshots them
// together — installing a config without its hooks leaves a Core holding a
// half-seam.
void ts2_install_game_config() {
  extern const GameHooks *ts2_game_hooks(); // game/core/game_hooks.cpp
  psxport_install_game(&g_ts2_cfg, ts2_game_hooks());
}
