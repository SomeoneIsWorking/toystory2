#include "render/mesh_culling.h"

namespace ts2 {

bool rowsOutside(CullRegisters &r, std::span<const std::uint32_t> corners) {
  r.v0 = corners[0] - r.at;
  if (!negative(r.v0)) {
    r.v0 = corners[0] - r.v1;
    r.at = corners[1] - r.v1;
    if (negative(r.v0)) {
      return false;
    }
    for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
      const bool inside = negative(r.at);
      r.at = corners[i + 1] - r.v1;
      if (inside) {
        return false;
      }
    }
    return !negative(r.at);
  }
  r.v1 = corners[1] - r.at;
  for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
    const bool inside = !negative(r.v1);
    r.v1 = corners[i + 1] - r.at;
    if (inside) {
      return false;
    }
  }
  return negative(r.v1);
}

bool columnsOutside(CullRegisters &r, std::span<const std::uint32_t> corners) {
  r.a3 = corners[0] << 16;
  r.v0 = r.a3 - r.at;
  if (!negative(r.v0)) {
    r.a3 -= r.v1;
    if (negative(r.a3)) {
      return false;
    }
    for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
      r.a3 = (corners[i] << 16) - r.v1;
      if (negative(r.a3)) {
        return false;
      }
    }
    r.a3 = (corners.back() << 16) - r.v1;
    return !negative(r.a3);
  }
  for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
    r.a3 = (corners[i] << 16) - r.at;
    if (!negative(r.a3)) {
      return false;
    }
  }
  r.a3 = (corners.back() << 16) - r.at;
  return negative(r.a3);
}

} // namespace ts2
