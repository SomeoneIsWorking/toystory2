// A resident mesh drawer's registers at entry and what it hands back, and the guest memory one call reads.
#pragma once

#include "render/memory_range.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ts2 {

// a0 is the mesh, a3 the view record; a2 is the ordering table (rigid drawer) or the bucket list selector
// (static submitter). `v1` and `ra` are the caller's: a call that draws nothing returns v1 unchanged. `s6` is
// the caller's too; the static submitter never sets it, and its split path stores it in a scratchpad frame.
struct MeshDrawCall {
  std::uint32_t drawer = 0;
  std::array<std::uint32_t, 4> args{};
  std::uint32_t v1 = 0;
  std::uint32_t ra = 0;
  std::uint32_t s6 = 0;
};

// The caller publishes one u16 per face entry at the table `*kInstanceSlotTable` before each call: 0 for none,
// else the word index of the face's packet in the packet array at `*kSlotPacketBase`.
inline constexpr std::uint32_t kInstanceSlotTable = 0x800A11CCu;
inline constexpr std::uint32_t kSlotPacketBase = 0x1F800044u;

struct MeshDrawResult {
  std::uint32_t v0 = 0;
  std::uint32_t v1 = 0;
};

// What a drawer reads from guest memory for one call, and the most packets it can link.
struct MeshDrawInputs {
  std::vector<MemoryRange> ranges;
  std::uint32_t meshBytes = 0; // the mesh through its terminator
  std::uint32_t primitives = 0;
  // Halfwords inside the ranges that the saved copy holds as 0: the end of a list the drawer pops from, so a
  // render that wants more than the call used finds the list ended.
  std::vector<std::uint32_t> terminators;
};

} // namespace ts2
