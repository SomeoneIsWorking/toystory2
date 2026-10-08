// SLUS_008.93 guest facts as the typed groups ToyStory2Runtime hands the framework. An unmeasured fact
// stays absent, because psxport fails fast on a zero but not on a plausible wrong address.
#pragma once

#include "cd/stock_libcd_layout.h"
#include "guest_cd_stream_callback_layout.h"
#include "guest_packet_pool_windows.h"
#include "guest_pad_buffer_layout.h"
#include "guest_program_image.h"
#include "overlay/shared_slot_image.h"
#include "platform_hle.h"

#include <cstdint>

namespace ts2::facts {

// From the SLUS_008.93 PS-EXE header and SYSTEM.CNF. The header stack (0x801FFFF0) and the SYSTEM.CNF
// stack (0x801FFF00) disagree; crt0 overwrites both with sp=fp=0x80200000 (0x80082E10 ORed with KSEG0).
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

// Graphics init 0x80039D9C builds the buffer objects at 0x801BBD28/0x801DD21C; the selected one publishes
// object+0x2C4 to gp+0x3E4 (0x800A10BC). The OT extent is unbound.
inline constexpr uint32_t kPacketPoolBase = 0x801BBFECu;
inline constexpr uint32_t kPacketPoolStride = 0x000214F4u;
inline constexpr uint32_t kCurrentPacketPoolPointer = 0x800A10BCu;
static_assert(kPacketPoolBase + kPacketPoolStride == 0x801DD4E0u,
              "the two measured render-buffer parities must retain the same packet-pool offset");
inline constexpr GuestPacketPoolWindows kPacketPoolWindows{
    .representation = GuestPacketPoolWindows::Representation::FixedBaseStride,
    .base = kPacketPoolBase,
    .stride = kPacketPoolStride,
};

inline constexpr uint32_t kCrt0BssZeroLo = 0x800A1070u;     // sw zero @ 0x80082D70
inline constexpr uint32_t kCrt0BssZeroHi = 0x800D12C0u;     // sltu bound @ 0x80082D78
inline constexpr uint32_t kCrt0StackTopBase = 0x80082E10u;  // lw v0 @ 0x80082DA4
inline constexpr uint32_t kCrt0StackTopBase2 = 0x800A0764u; // lw v1 @ 0x80082DC4
inline constexpr uint32_t kCrt0HeapBase = 0x800D12C0u;      // sll/srl mask @ 0x80082DB8
// Zero means absent: the walk reaches InitHeap after one absolute non-BSS store (`sw ra,4208(at)`),
// and none writes the heap size or base.
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

// FUN_8003D88C picks a level.bin/levelN.bin and passes 0x800D12C0 to loader FUN_80082508. BITS/MEMORY.BIN
// may load first at 0x800D5D20, so the LEVEL window is 19,040 bytes and five of ten pairs share one slot.
inline constexpr uint32_t kLevelOverlayBase = 0x800D12C0u;
inline constexpr uint32_t kMemoryOverlayBase = ts2::SharedSlotImage::kLoadAddress;
static_assert(kLevelOverlayBase == kCrt0HeapBase,
              "the level overlay slot starts at the independently verified crt0 heap base");
static_assert(kMemoryOverlayBase - kLevelOverlayBase == 19040u,
              "the next co-resident slot bounds the level overlay window");

// Physical resident range from the PS-X EXE header, the image identity for psxport's dynarec.
inline constexpr uint32_t kRecMainLo = 0x00010000u;
inline constexpr uint32_t kRecMainHi = 0x000A1800u;

// Pad init at 0x8003EF20 stores its two buffer pointers into 0xF0-byte driver contexts: [0x800A3E98]
// holds slot 0 (0x800CF8A0) and [0x800A3F88] holds slot 1 (0x800CF8C8). The decoder at 0x8003AC58 reads 0x800CF8A0.
inline constexpr uint32_t kPadSlot0Buffer = 0x800CF8A0u;
inline constexpr uint32_t kPadSlot1Buffer = 0x800CF8C8u;
inline constexpr uint32_t kPadDriverPointerTable = 0x800A3E98u;
inline constexpr uint32_t kPadDriverContextStride = 0xF0u;
static_assert(kPadDriverPointerTable + kPadDriverContextStride == 0x800A3F88u,
              "the measured per-port driver pointer fields must stay one context apart");

// The graphics initializer at 0x8003A650 calls SetGeomOffset (0x80083CD4) and SetGeomScreen (0x80083CF4)
// with (OFX,OFY,H)=(256,120,160); PlatformHle replaces both and records the projection in ProjParams.
inline constexpr uint32_t kProjectionLeavesLo = 0x80083CD4u;
inline constexpr uint32_t kProjectionLeavesHi = 0x80083D00u;
inline constexpr uint32_t kSetGeomOffset = 0x80083CD4u;
inline constexpr uint32_t kSetGeomScreen = 0x80083CF4u;
static_assert(kSetGeomOffset >= kProjectionLeavesLo && kSetGeomOffset < kProjectionLeavesHi);
static_assert(kSetGeomScreen >= kProjectionLeavesLo && kSetGeomScreen < kProjectionLeavesHi);

// libgpu's DMA timeout arm/check leaves query VSync(-1), but the host GPU is synchronous, so the
// framework publishes these globals without entering the guest bodies.
inline constexpr uint32_t kGpuTimeoutArm = 0x80088380u;
inline constexpr uint32_t kGpuTimeoutCheck = 0x800883B4u;
inline constexpr uint32_t kGpuTimeoutDeadline = 0x8009EC20u;
inline constexpr uint32_t kGpuTimeoutFlag = 0x8009EC24u;

// VSync spans 0x80088628..0x80088770. FrameDriver owns the display fields, so any guest call traps.
inline constexpr uint32_t kVSyncBodyLo = 0x80088628u;
inline constexpr uint32_t kVSyncBodyHi = 0x80088770u;
inline constexpr uint32_t kVSyncTrap = 0x80088628u;
// VSync with a negative argument returns this word (0x80088660 bgez not taken, 0x8008866C lw
// 0x800A0000-0x2AC, exit via 0x80088758). FrameDriver mirrors the host field count into it.
inline constexpr uint32_t kVSyncQueryCounter = 0x8009FD54u;
static_assert(kVSyncTrap >= kVSyncBodyLo && kVSyncTrap < kVSyncBodyHi);

// Both are linked SCEI leaves; merging their ranges frees the second PlatformHle window for stock libcd.
inline constexpr uint32_t kSdkGraphicsWindowLo = kProjectionLeavesLo;
inline constexpr uint32_t kSdkGraphicsWindowHi = kVSyncBodyHi;
static_assert(kSetGeomOffset >= kSdkGraphicsWindowLo && kSetGeomOffset < kSdkGraphicsWindowHi);
static_assert(kVSyncTrap >= kSdkGraphicsWindowLo && kVSyncTrap < kSdkGraphicsWindowHi);

// This port's disc key, checked before PSXPORT_DISC.
inline constexpr const char *kDiscEnvVar = "PSXPORT_TS2_DISC";

// Window title and memory-card policy; the card variable is checked before PSXPORT_CARD.
inline constexpr const char *kWindowTitle = "Toy Story 2 (psxport)";
inline constexpr const char *kCardEnvVar = "PSXPORT_TS2_CARD";
inline constexpr const char *kCardDefaultPath = "scratch/saves/toystory2.mcr";

// Consumed as one group by the crt0 planner. `stackBias` is 0: no bias instruction between the lw and
// the or into sp.
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

// The retail initializer registers the two buffers in per-port contexts; serviceFrame reads those first
// and falls back to the fixed buffers.
inline constexpr GuestPadBufferLayout kPadBufferLayout{
    .slot0Buffer = kPadSlot0Buffer,
    .slot1Buffer = kPadSlot1Buffer,
    .slotPointerTable = kPadDriverPointerTable,
    .slotPointerStride = kPadDriverContextStride,
};

// libcd's data-ready interrupt goes through the guest's CdReadyCallback slot; the framework calls its
// current value on INT1. `stockReadRaisesCompletion` stays false: a stock CdRead is polled via CdReadSync.
inline constexpr GuestCdStreamCallbackLayout kCdStreamCallbackLayout{
    .readyCallbackPointer = cd::kStockLibcdLayout.readyCallback,
    .owner = GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt,
};

// libapi's per-channel DMA callback table, scanned by IRQ3 dispatcher 0x800890C4 at
// `[kDmaCallbackTable + 4*ch]`. The framework calls it itself, so 0x800890C4 must not also run.
inline constexpr std::uint32_t kDmaCallbackTable = 0x8009FD60u;

// Two SCEI-library windows: projection leaves plus the VSync trap, and the stock-libcd span.
// Registration is exact-address, so other library bodies stay guest code.
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
