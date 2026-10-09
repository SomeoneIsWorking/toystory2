#include "render/ordering_tables.h"

#include "core.h"
#include "facts/guest_facts.h"

namespace ts2 {

// ClearOTagR links bucket i to bucket i - 1, so the walk starts at the last bucket and runs down.
void OrderingTables::name(Core &core) {
  for (const std::uint32_t buffer : {facts::kGraphicsBufferA, facts::kGraphicsBufferB}) {
    core.otTables.name(kTableId,
                       buffer + facts::kOrderingTableOffset,
                       facts::kOrderingTableBuckets,
                       sizeof(std::uint32_t),
                       psx::gpu::OtWalk::HighToLow);
  }
}

} // namespace ts2
