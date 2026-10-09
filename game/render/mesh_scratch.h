// The scratchpad words the resident mesh drawers share with the caller that publishes their per-call state.
#pragma once

#include <cstdint>

namespace ts2 {

inline constexpr std::uint32_t kMeshScratch = 0x1F800000u;
inline constexpr std::uint32_t kMeshScratchBytes = 0x400u;

namespace mesh_scratch {

inline constexpr std::uint32_t kClutHigh = kMeshScratch + 0x20u; // texture page entry's CLUT half, low half cleared
inline constexpr std::uint32_t kPageWord = kMeshScratch + 0x24u; // texture page in its high half
inline constexpr std::uint32_t kPageLow = kMeshScratch + 0x26u;
inline constexpr std::uint32_t kSemiMode = kMeshScratch + 0x2Cu;
inline constexpr std::uint32_t kQuadCommand = kMeshScratch + 0x30u;
inline constexpr std::uint32_t kTriangleCommand = kMeshScratch + 0x34u;
inline constexpr std::uint32_t kNearLimit = kMeshScratch + 0x38u;
inline constexpr std::uint32_t kAltPackets = kMeshScratch + 0x48u; // the other buffer's packet array, written alongside
inline constexpr std::uint32_t kReleased = kMeshScratch + 0x4Cu;   // cursor into the list of slots handed back
inline constexpr std::uint32_t kFree = kMeshScratch + 0x50u;       // cursor into the list of free slots
inline constexpr std::uint32_t kColumnMin = kMeshScratch + 0x60u;
inline constexpr std::uint32_t kColumnMax = kMeshScratch + 0x64u;
inline constexpr std::uint32_t kRowMin = kMeshScratch + 0x68u;
inline constexpr std::uint32_t kRowMax = kMeshScratch + 0x6Cu;
inline constexpr std::uint32_t kCorners = kMeshScratch + 0x70u; // the next primitive's vertex addresses
inline constexpr std::uint32_t kClutMode = kMeshScratch + 0x80u;

} // namespace mesh_scratch
} // namespace ts2
