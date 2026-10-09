// The three actor part drawers, ported: a face that survives the depth, facing, row and bucket tests becomes
// one GT4/GT3 packet, and a culled face still leaves the stores the guest made before the test that dropped it.
// One body serves the guest override and the render of a saved call.
#pragma once

#include "emit_memory.h"

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace ts2 {

// A drawer's registers at entry: a0 is the part record, a1..a3 the drawer's own arguments.
struct PartDrawCall {
  std::uint32_t drawer = 0;
  std::array<std::uint32_t, 4> args{};
};

struct MemoryRange {
  std::uint32_t address = 0;
  std::uint32_t size = 0;
};

// What a drawer reads from guest memory for one call, and the most packet bytes it can write.
struct PartDrawInputs {
  std::vector<MemoryRange> ranges;
  std::uint32_t packetBytes = 0;
};

class PartFaceDrawers {
public:
  inline static constexpr std::uint32_t kPrelit = 0x8001C920u;
  inline static constexpr std::uint32_t kNormalLit = 0x8001DFE4u;
  // 0x8001CD34 (DCPL-lit) stays on the guest: no replay reaches it, so a native port could not be diff-proven.
  // The packet cursor and ordering table base every drawer reads and the cursor it advances.
  inline static constexpr std::uint32_t kPacketCursor = 0x800A1608u;
  inline static constexpr std::uint32_t kOrderingTable = 0x800A10BCu;

  static void install(Core &core);

  // The drawer `call.drawer` over `memory`: the guest's packets and ordering table links, or host bytes.
  static void run(const psx::present::EmitMemory &memory, const PartDrawCall &call);

  // Read from `memory` before `run`, so the call can be run again over host copies.
  static PartDrawInputs inputs(const psx::present::EmitMemory &memory, const PartDrawCall &call);
};

} // namespace ts2
