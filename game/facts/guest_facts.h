// guest_facts.h — measured Toy Story 2 (SLUS_008.93, USA) guest facts, declared as the typed groups
// ToyStory2Runtime hands the framework. There is no legacy GameConfig here: each group is the narrow
// immutable struct psxport defines for it (GuestProgramImage, PlatformHlePlan, GuestPadBufferLayout,
// GuestCdStreamCallbackLayout).
//
// READ THIS BEFORE FILLING ANYTHING IN.
//
// Only complete measured groups are filled (RE-01 crt0, RE-02's physical resident range, RE-03
// overlay slots, RE-06 pad routing, RE-07's projection leaves, RE-18 timeout/VSync, RE-04 stock
// libcd). A fact that is not measured is left absent deliberately: psxport fails fast on a zero it
// needs, whereas a plausible-looking WRONG address does not fail cleanly — it breaks boot or diverges
// in a way that reads as a framework bug. Each group cites the bytes it came from.
//
// THERE IS NO DECOMP of Toy Story 2: no symbol map, no function boundaries, no matching build.
// Every address here comes from reproducible binary evidence on SLUS_008.93 in this repo; when you
// fill one, re-derive it from the executable bytes before you trust it.
#pragma once

#include "cd/stock_libcd_layout.h"
#include "guest_cd_stream_callback_layout.h"
#include "guest_pad_buffer_layout.h"
#include "guest_program_image.h"
#include "overlay/shared_slot_image.h"
#include "platform_hle.h"

#include <cstdint>

namespace ts2::facts {

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
inline constexpr uint32_t kPsExeEntry = 0x80082D60u;     // header pc0
inline constexpr uint32_t kPsExeTextAddr = 0x80010000u;  // header t_addr
inline constexpr uint32_t kPsExeTextSize = 0x00091800u;  // header t_size (595,968 B)
inline constexpr uint32_t kPsExeSpHeader = 0x801FFFF0u;  // header s_addr
inline constexpr uint32_t kSystemCnfStack = 0x801FFF00u; // SYSTEM.CNF STACK=, which wins at boot
static_assert(kPsExeEntry >= kPsExeTextAddr && kPsExeEntry < kPsExeTextAddr + kPsExeTextSize,
              "the PS-EXE entry must lie inside the loaded text — if this fires, the "
              "header was "
              "misread and every number in this file's comment block is suspect");
static_assert(kSystemCnfStack < kPsExeSpHeader,
              "the two stack values are recorded because they DISAGREE "
              "(SYSTEM.CNF 0x801FFF00 vs header "
              "0x801FFFF0); if this ever fires, one of them was re-read and "
              "RE-01's note is stale");

// RE-05 (partial). Resident graphics init 0x80039D9C constructs the two buffer objects at
// 0x801BBD28/0x801DD21C. The selected buffer publishes object+0x2C4 to gp+0x3E4 (0x800A10BC), making
// these values direct retail facts. OT extent and the remaining submit leaves stay unbound below.
inline constexpr uint32_t kPacketPoolBase = 0x801BBFECu;
inline constexpr uint32_t kPacketPoolStride = 0x000214F4u;
inline constexpr uint32_t kCurrentPacketPoolPointer = 0x800A10BCu;
static_assert(kPacketPoolBase + kPacketPoolStride == 0x801DD4E0u,
              "the two measured render-buffer parities must retain the same packet-pool offset");

// RE-01, measured from the verified executable. The boot group is followed symbolically from entry
// 0x80082D60 through the InitHeap return, second jal and terminating break; every value below is
// re-derivable from the identity-checked PS-X EXE (tools/extract_exe.py identifies it).
// a cross-binary negative.
inline constexpr uint32_t kCrt0BssZeroLo = 0x800A1070u;     // sw zero @ 0x80082D70
inline constexpr uint32_t kCrt0BssZeroHi = 0x800D12C0u;     // sltu bound @ 0x80082D78
inline constexpr uint32_t kCrt0StackTopBase = 0x80082E10u;  // lw v0 @ 0x80082DA4
inline constexpr uint32_t kCrt0StackTopBase2 = 0x800A0764u; // lw v1 @ 0x80082DC4
inline constexpr uint32_t kCrt0HeapBase = 0x800D12C0u;      // sll/srl mask @ 0x80082DB8
// A complete walk reaches InitHeap after exactly one absolute non-BSS store,
// `sw ra,4208(at)`. No store writes the computed heap size or base, so zero is
// the measured meaning ABSENT for these optional framework fields.
inline constexpr uint32_t kCrt0HeapSizePtr = 0u;
inline constexpr uint32_t kCrt0HeapBasePtr = 0u;
inline constexpr uint32_t kCrt0Gp = 0x800A0CD8u;       // addiu gp @ 0x80082DE4
inline constexpr uint32_t kCrt0LibcInit = 0x80089344u; // jal @ 0x80082DEC; A(39h) thunk
inline constexpr uint32_t kCrt0GameMain = 0x8007A9E8u; // second jal @ 0x80082E00
inline constexpr uint32_t kCrt0Entry = kPsExeEntry;
inline constexpr int32_t kCrt0StackBias = 0; // no bias instruction between lw and or sp
static_assert(kCrt0BssZeroHi == kCrt0HeapBase, "this crt0 starts its heap exactly at the proven BSS upper bound");
static_assert(kCrt0StackTopBase >= kPsExeTextAddr && kCrt0StackTopBase < kPsExeTextAddr + kPsExeTextSize,
              "the stack-top word must be readable from the loaded executable image");

// ─────────────────────────────────────────────────────────────────────────────────────────────────────
// THE OVERLAY MAP, DECODED FROM THE LOADER'S OWN CALL SITES (tools/overlay_map.py).
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
inline constexpr uint32_t kLevelOverlayBase = 0x800D12C0u;
inline constexpr uint32_t kMemoryOverlayBase = ts2::SharedSlotImage::kLoadAddress;
static_assert(kLevelOverlayBase == kCrt0HeapBase,
              "the level overlay slot starts at the independently verified crt0 heap base");
static_assert(kMemoryOverlayBase - kLevelOverlayBase == 19040u,
              "the next co-resident slot bounds the level overlay window");

// Physical resident range from the identity-checked PS-X EXE header. Despite the bounded legacy
// field names below, this is runtime image identity for psxport's dynarec, not a static code corpus.
inline constexpr uint32_t kRecMainLo = 0x00010000u;
inline constexpr uint32_t kRecMainHi = 0x000A1800u;

// RE-06, measured from the identity-checked retail executable.
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
inline constexpr uint32_t kPadSlot0Buffer = 0x800CF8A0u;
inline constexpr uint32_t kPadSlot1Buffer = 0x800CF8C8u;
inline constexpr uint32_t kPadDriverPointerTable = 0x800A3E98u;
inline constexpr uint32_t kPadDriverContextStride = 0xF0u;
static_assert(kPadDriverPointerTable + kPadDriverContextStride == 0x800A3F88u,
              "the measured per-port driver pointer fields must stay one context apart");

// RE-07 (partial), measured from the identity-checked retail executable.
// The graphics initializer at 0x8003A650 calls these linked
// libgte leaves with (OFX,OFY,H)=(256,120,160):
//
//   0x80083CD4  SetGeomOffset: sll a0/a1 by 16, ctc2 to CR24/CR25, return
//   0x80083CF4  SetGeomScreen: ctc2 a0 to CR26, return
//
// PlatformHle replaces only those two exact leaves with psxport's common implementations. Those
// implementations preserve the retail GTE writes and additionally record the authored projection in
// this Core's ProjParams. The half-open window covers the measured leaf bodies and their alignment
// padding only; it does not make an unsupported claim about the rest of the linked Sony library.
inline constexpr uint32_t kProjectionLeavesLo = 0x80083CD4u;
inline constexpr uint32_t kProjectionLeavesHi = 0x80083D00u;
inline constexpr uint32_t kSetGeomOffset = 0x80083CD4u;
inline constexpr uint32_t kSetGeomScreen = 0x80083CF4u;
static_assert(kSetGeomOffset >= kProjectionLeavesLo && kSetGeomOffset < kProjectionLeavesHi);
static_assert(kSetGeomScreen >= kProjectionLeavesLo && kSetGeomScreen < kProjectionLeavesHi);

// RE-18: libgpu arms/checks its DMA timeout through two exact linked-library leaves. Their guest
// bodies query VSync(-1), but the host GPU consumes command/DMA work synchronously. The framework's
// standard owner therefore publishes the measured globals without entering either guest body.
inline constexpr uint32_t kGpuTimeoutArm = 0x80088380u;
inline constexpr uint32_t kGpuTimeoutCheck = 0x800883B4u;
inline constexpr uint32_t kGpuTimeoutDeadline = 0x8009EC20u;
inline constexpr uint32_t kGpuTimeoutFlag = 0x8009EC24u;

// RE-18, measured from the identity-checked linked libetc body and its 56 static call sites. VSync
// starts at 0x80088628 and ends at the next helper 0x80088770; its timeout helper references the
// retail "VSync: timeout" string. The title FrameDriver now bypasses resident wait 0x8003FA68 and
// owns the two display fields itself, so every remaining guest call is an ownership violation and
// traps. This second half-open window admits only the measured VSync body.
inline constexpr uint32_t kVSyncBodyLo = 0x80088628u;
inline constexpr uint32_t kVSyncBodyHi = 0x80088770u;
inline constexpr uint32_t kVSyncTrap = 0x80088628u;
// The word VSync returns for a negative argument: 0x80088660 `bgez $a0,0x80088678` is not taken, so it
// falls to 0x80088668 `lui $v0,0x800a` / 0x8008866C `lw $v0,-0x2ac($v0)` (0x800A0000 - 0x2AC) and
// returns through 0x80088670 `j 0x80088758`. The title FrameDriver mirrors the host field count into
// it; the linked CdSync/CdControl timeouts read it through VSync(-1) and nothing waits on it.
inline constexpr uint32_t kVSyncQueryCounter = 0x8009FD54u;
static_assert(kVSyncTrap >= kVSyncBodyLo && kVSyncTrap < kVSyncBodyHi);

// The projection and VSync functions are both linked SCEI library leaves. Combining their adjacent
// address ranges frees the second PlatformHle window for the measured stock-libcd group without
// admitting any additional handler: PlatformHle still registers exact entry points only.
inline constexpr uint32_t kSdkGraphicsWindowLo = kProjectionLeavesLo;
inline constexpr uint32_t kSdkGraphicsWindowHi = kVSyncBodyHi;
static_assert(kSetGeomOffset >= kSdkGraphicsWindowLo && kSetGeomOffset < kSdkGraphicsWindowHi);
static_assert(kVSyncTrap >= kSdkGraphicsWindowLo && kVSyncTrap < kSdkGraphicsWindowHi);

// This port's own disc key (a port fact, not RE): the framework resolver checks this variable in the
// environment and ./.env before the generic PSXPORT_DISC. tools/resolve_disc.py implements the same
// key on the host side and .env.example documents it.
inline constexpr const char *kDiscEnvVar = "PSXPORT_TS2_DISC";

// The host window title and the memory-card policy (port facts): the card variable is checked before
// the generic PSXPORT_CARD, and the default path applies when neither names a card.
inline constexpr const char *kWindowTitle = "Toy Story 2 (psxport)";
inline constexpr const char *kCardEnvVar = "PSXPORT_TS2_CARD";
inline constexpr const char *kCardDefaultPath = "scratch/saves/toystory2.mcr";

// The executable's own boot group (RE-01), resident text (RE-02) and crt0 stack bias, consumed as one
// group by the framework's crt0 planner: a lone entry beside a zeroed BSS range would run a wrong crt0
// instead of refusing. Every value is re-derivable from the instruction stream of the identity-checked
// executable. `stackBias` is declared with value 0: no bias instruction exists between the lw and the
// or into sp.
inline constexpr GuestProgramImage kProgramImage{
    .bss = {kCrt0BssZeroLo, kCrt0BssZeroHi},
    .stackTopWordAddress = kCrt0StackTopBase,
    .stackReserveWordAddress = kCrt0StackTopBase2,
    .heapBase = kCrt0HeapBase,
    .heapSizeStoreAddress = kCrt0HeapSizePtr,
    .heapBaseStoreAddress = kCrt0HeapBasePtr,
    .globalPointer = kCrt0Gp,
    .libcInitEntry = kCrt0LibcInit,
    .gameMainEntry = kCrt0GameMain,
    .crt0Entry = kCrt0Entry,
    .residentText = {kRecMainLo, kRecMainHi},
    .backtraceText = {},
    .stackBias = {true, kCrt0StackBias},
};

// The retail initializer takes the two fixed buffers directly and registers them in per-port contexts,
// so serviceFrame consults the measured pointer fields first and falls back to the fixed buffers if
// the guest has not initialized the contexts yet (RE-06).
inline constexpr GuestPadBufferLayout kPadBufferLayout{
    .slot0Buffer = kPadSlot0Buffer,
    .slot1Buffer = kPadSlot1Buffer,
    .slotPointerTable = kPadDriverPointerTable,
    .slotPointerStride = kPadDriverContextStride,
};

// libcd delivers its data-ready interrupt through the guest's registered ready callback
// (CdReadyCallback slot, RE-04). The framework stands in for the BIOS CD-ROM interrupt handler:
// when the controller raises INT1 it acknowledges the response and calls the CURRENT value of this
// slot. `stockReadRaisesCompletion` stays false, deliberately: a synchronous stock CdRead is a data
// load whose caller polls CdReadSync, and libstr's per-sector callback must run once per STREAMED
// sector (the ReadS the controller drives), never once per stock read.
inline constexpr GuestCdStreamCallbackLayout kCdStreamCallbackLayout{
    .readyCallbackPointer = cd::kStockLibcdLayout.readyCallback,
    .owner = GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt,
};

// libapi's per-channel DMA callback table. Its IRQ3 dispatcher 0x800890C4 (installed as the
// InterruptCallback(3) handler in the 0x8009EC9C table) scans the DICR channel flags and calls
// `[kDmaCallbackTable + 4*ch]` with `lui 0x800a; addiu -0x2a0` (0x80089110/14) as `$s5`, advancing
// 4 bytes per channel (0x8008916C). The libcd data-end callback 0x80093E88 reaches channel 3 through
// the 0x80091148 `DMACallback` wrapper. The framework stands in for the BIOS DMA handler by calling
// the current word of this table directly, so it must not also run 0x800890C4, which would find the
// channel flag already acknowledged and call nothing.
inline constexpr std::uint32_t kDmaCallbackTable = 0x8009FD60u;

// The hardware-sync primitives this binary links, and the windows that admit them. One SCEI-library
// window owns the projection leaves and the mandatory VSync trap (RE-07/RE-18); the other admits only
// the measured stock-libcd span. Registration remains exact-address, so unrelated library bodies
// remain guest code. The window and entry facts below are all consumed through the plan; no handler
// lives in the title.
inline constexpr PlatformHlePlan kPlatformHlePlan{
    .setGeomOffset = kSetGeomOffset,
    .setGeomScreen = kSetGeomScreen,
    .cdReadAddress = cd::kStockLibcdLayout.read,
    .cdReadSyncAddress = cd::kStockLibcdLayout.readSync,
    .cdCommandAddress = cd::kStockLibcdLayout.command,
    .cdSyncAddress = cd::kStockLibcdLayout.sync,
    .cdSearchFileAddress = cd::kStockLibcdLayout.searchFile,
    .cdGetSectorAddress = cd::kStockLibcdLayout.getSector,
    .stockCdWorkArea = {cd::kStockLibcdLayout.lastPosition, cd::kStockLibcdLayout.lastPosition + 4u},
    .gpuTimeoutArmAddress = kGpuTimeoutArm,
    .gpuTimeoutCheckAddress = kGpuTimeoutCheck,
    .gpuTimeoutDeadlineVar = kGpuTimeoutDeadline,
    .gpuTimeoutFlagVar = kGpuTimeoutFlag,
    .dmaCallbackTable = kDmaCallbackTable,
    .vsyncAddress = kVSyncTrap,
    .vsyncQueryCounterAddress = kVSyncQueryCounter,
    .windowLo = {kSdkGraphicsWindowLo, cd::kStockLibcdLayout.libraryWindowLo},
    .windowHi = {kSdkGraphicsWindowHi, cd::kStockLibcdLayout.libraryWindowHi},
};

} // namespace ts2::facts
