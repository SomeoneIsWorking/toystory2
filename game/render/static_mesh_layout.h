// Guest addresses the static mesh submitter reads or writes beyond the scratchpad words it shares with the
// rigid drawer (mesh_scratch.h).
#pragma once

#include "render/mesh_scratch.h"

#include <cstdint>

namespace ts2 {

inline constexpr std::uint32_t kBucketTables = 0x800A12F8u; // pointer to the per-table lists of bucket offsets
inline constexpr std::uint32_t kOtzLimit = 0x800A135Cu;     // deepest OTZ, in words, a primitive may sit at

namespace static_scratch {

inline constexpr std::uint32_t kOrderingTable = kMeshScratch + 0x40u;
inline constexpr std::uint32_t kDrawn = kMeshScratch + 0x28u; // 1 until a primitive is linked
inline constexpr std::uint32_t kReturn = kMeshScratch + 0x54u;
inline constexpr std::uint32_t kLodChanged = kMeshScratch + 0xA0u;
inline constexpr std::uint32_t kLodEnabled = kMeshScratch + 0x82u; // 1 when the command's page select is under 0x40
inline constexpr std::uint32_t kVertices = kMeshScratch + 0xE4u;   // the current primitive's vertex addresses

} // namespace static_scratch
} // namespace ts2
