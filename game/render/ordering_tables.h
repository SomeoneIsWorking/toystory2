// The two ClearOTagR tables of the graphics buffers, named so the OT walk tags each recorded packet with its bucket.
#pragma once

#include <cstdint>

class Core;

namespace ts2 {

class OrderingTables {
public:
  // One id for both buffers: only the displayed one is walked per frame.
  inline static constexpr std::uint16_t kTableId = 1;

  static void name(Core &core);
};

} // namespace ts2
