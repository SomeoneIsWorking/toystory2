// The scratchpad words the textured split parks values in, and the uv mean they share.
#pragma once

#include "render/mesh_scratch.h"

#include <cstdint>

namespace ts2 {

inline constexpr std::uint32_t kSplitCarries = kMeshScratch + 136u;
inline constexpr std::uint32_t kSplitSavedT8 = kMeshScratch + 156u;

// The mean of two uv halves: each is halved with the low bit of both bytes cleared, and the carry is what those
// dropped bits would have summed to.
inline std::uint32_t meanUv(std::uint32_t a, std::uint32_t b, std::uint32_t carry) {
  return ((((a & 0xFEFEu) + (b & 0xFEFEu)) >> 1) & 0xFEFEu) + carry;
}

} // namespace ts2
