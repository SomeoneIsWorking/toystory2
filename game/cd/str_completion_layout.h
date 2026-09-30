#pragma once

#include <cstdint>

namespace ts2::cd {

// The STR stream ring the FMV player blocks on, and the one routine that can satisfy it.
//
// This is a distinct fact from StockLibcdLayout: those are the library ENTRIES the port owns
// natively, and this is the ring STATE those routines and the guest's own code share. Every
// address and instruction word below is verified against the retail executable and the retail
// module images by tools/verify_str_completion.py, which refuses rather than reporting a stale
// conclusion; the citations are that tool's CHAIN table.
struct StrCompletionLayout {
  uint32_t ringBaseSlot;  // [0x800CE1B0] the ring base pointer
  uint32_t ringHeadSlot;  // [0x800C9504] the slot index, always 0 in the measured runs
  uint32_t postGuardA;    // [0x800CE148] gate 1: zero means the post is skipped
  uint32_t postGuardB;    // [0x800C1170] gate 2: the end-of-stream flag
  uint32_t userCallback;  // [0x800B2364] the optional callback the post invokes
  uint32_t popEntry;      // 0x800940F4 the ring pop the FMV waits on
  uint32_t postEntry;     // 0x80093E88 the completion post, the only writer of state 2
  uint32_t postCallSite;  // 0x80094B30 the only call to the post
  uint32_t entryStride;   // 32 bytes, shared: both sides use the encoding 0x00021140
  uint32_t completeState; // the state word the post writes and the pop accepts: 2
};

inline constexpr StrCompletionLayout kStrCompletionLayout{
    // 0x80090808 sw $a0,-0x1e50($at) -- the ring init's single store, and the image's only writer.
    .ringBaseSlot = 0x800CE1B0u,
    // 0x800940FC lw $v0,-0x6afc($v0) over `lui $v0,0x800d`; 0x800D0000 - 0x6AFC.
    .ringHeadSlot = 0x800C9504u,
    // 0x80094B14 beqz $v1,0x80094b38 over `lw $v1,-0x1eb8($v1)`; 0x800D0000 - 0x1EB8.
    .postGuardA = 0x800CE148u,
    // 0x80094B28 beqz $v0,0x80094b38 over `lw $v0,0x1170($v0)` over `lui $v0,0x800c`.
    .postGuardB = 0x800C1170u,
    // 0x80093EDC lw $a0,0x2364($a0) over `lui $a0,0x800b`; the stream open writes it at 0x80093FB0.
    .userCallback = 0x800B2364u,
    .popEntry = 0x800940F4u,
    .postEntry = 0x80093E88u,
    .postCallSite = 0x80094B30u,
    .entryStride = 32u,
    .completeState = 2u,
};

// The FMV overlay's own wait: a bounded spin whose retry count is 0x8000 = 32768, verified at
// 0x800D6DD8 `lui $s0,0x80`, 0x800D6DEC `beqz $v0` and 0x800D6DFC the exhaustion jump. The count
// is why this is an UNDELIVERED EVENT and not a long body: the guest gives up on its own.
inline constexpr uint32_t kFmvWaitRetries = 0x8000u;
inline constexpr uint32_t kFmvWaitEntry = 0x800D6DC8u;

} // namespace ts2::cd
