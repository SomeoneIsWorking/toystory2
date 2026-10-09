// The screen tests every resident mesh primitive passes before it is linked, with the temporaries the guest
// leaves behind: a drawer returns v1, so the last primitive's path shows in it.
#pragma once

#include "core.h"
#include "gte_registers.h"

#include <cstdint>
#include <span>

namespace ts2 {

inline bool negative(std::uint32_t value) {
  return static_cast<std::int32_t>(value) < 0;
}

inline bool below(std::uint32_t value, std::uint32_t limit) {
  return static_cast<std::int32_t>(value) < static_cast<std::int32_t>(limit);
}

inline std::uint32_t gteRead(std::uint32_t reg) {
  return gte_read_data(reg);
}

inline void gteWrite(std::uint32_t reg, std::uint32_t value) {
  gte_write_data(reg, value);
}

inline constexpr std::uint32_t kSz0 = 16u;
inline constexpr std::uint32_t kSz2 = 18u;
inline constexpr std::uint32_t kSz3 = 19u;

// BGR555 to the colour word the packet holds, red and blue swapped as the guest does. Only the first corner
// masks the low five bits; the others shift the whole sign-extended halfword, so its upper bits spill into the
// word's top byte.
inline std::uint32_t packColour(std::int32_t half, bool first) {
  const auto bits = static_cast<std::uint32_t>(half);
  return ((bits & 0x7C00u) >> 7) + ((bits & 0x3E0u) << 6) + ((first ? bits & 0x1Fu : bits) << 19);
}

struct CullRegisters {
  std::uint32_t at = 0;
  std::uint32_t v0 = 0;
  std::uint32_t v1 = 0;
  std::uint32_t a3 = 0;
};

// Whether every corner (SXY words compared whole, rows in the high half) lies above `at` or all lie below `v1`.
// The guest computes the next corner's difference in each branch's delay slot, so on leaving early the
// temporaries already hold it.
bool rowsOutside(CullRegisters &r, std::span<const std::uint32_t> corners);

// The same over the x halves, shifted to the top of the word.
bool columnsOutside(CullRegisters &r, std::span<const std::uint32_t> corners);

} // namespace ts2
