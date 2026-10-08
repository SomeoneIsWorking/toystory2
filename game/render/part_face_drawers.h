// The three actor part drawers, ported: a face that survives the depth, facing, row and bucket tests becomes
// one GT4/GT3 packet, and a culled face still leaves the stores the guest made before the test that dropped it.
#pragma once

#include <cstdint>

class Core;

namespace ts2 {

class PartFaceDrawers {
public:
  inline static constexpr std::uint32_t kPrelit = 0x8001C920u;
  inline static constexpr std::uint32_t kLit = 0x8001CD34u;
  inline static constexpr std::uint32_t kNormalLit = 0x8001DFE4u;

  static void install(Core &core);
};

} // namespace ts2
