// A run of guest memory a drawer reads, kept so the drawer can run again over host copies.
#pragma once

#include "emit_memory.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ts2 {

struct MemoryRange {
  std::uint32_t address = 0;
  std::uint32_t size = 0;
};

// The ranges as disjoint runs in address order; touching and overlapping ranges become one.
inline std::vector<MemoryRange> unionOf(std::vector<MemoryRange> ranges) {
  std::sort(ranges.begin(), ranges.end(), [](const MemoryRange &a, const MemoryRange &b) {
    return a.address < b.address;
  });
  std::vector<MemoryRange> merged;
  for (const MemoryRange &range : ranges) {
    if (!merged.empty() && range.address <= merged.back().address + merged.back().size) {
      const std::uint32_t end = std::max(merged.back().address + merged.back().size, range.address + range.size);
      merged.back().size = end - merged.back().address;
    } else {
      merged.push_back(range);
    }
  }
  return merged;
}

// The ranges' bytes from the guest, back to back in range order.
std::vector<std::byte> readRanges(const psx::present::EmitMemory &guest, std::span<const MemoryRange> ranges);

// Host copies of ranges read by `readRanges`.
void provideRanges(psx::present::HostMemory &host,
                   std::span<const MemoryRange> ranges,
                   std::span<const std::byte> bytes);

} // namespace ts2
