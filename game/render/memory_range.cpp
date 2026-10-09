#include "render/memory_range.h"

namespace ts2 {

std::vector<std::byte> readRanges(const psx::present::EmitMemory &guest, std::span<const MemoryRange> ranges) {
  std::vector<std::byte> bytes;
  for (const MemoryRange &range : ranges) {
    std::uint32_t offset = 0;
    for (; offset + 4u <= range.size; offset += 4u) {
      const std::uint32_t word = guest.mem_r32(range.address + offset);
      for (std::uint32_t byte = 0; byte != 4u; ++byte) {
        bytes.push_back(static_cast<std::byte>(word >> (byte * 8u)));
      }
    }
    for (; offset != range.size; ++offset) {
      bytes.push_back(static_cast<std::byte>(guest.mem_r8(range.address + offset)));
    }
  }
  return bytes;
}

void provideRanges(psx::present::HostMemory &host,
                   std::span<const MemoryRange> ranges,
                   std::span<const std::byte> bytes) {
  std::size_t at = 0;
  for (const MemoryRange &range : ranges) {
    host.provide(range.address, bytes.subspan(at, range.size));
    at += range.size;
  }
}

} // namespace ts2
