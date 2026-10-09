// What differs between the textured and the plain subdividers' split path: the packet's field spacing, its
// command words and the return addresses the guest's calls leave in ra.
#pragma once

#include "render/mesh_cpu.h"
#include "render/slot_release.h"

#include <array>
#include <cstdint>

namespace ts2 {

// One return address per child, for each kind of call the split path makes.
struct SplitSites {
  std::array<std::uint32_t, 4> subdivide; // the recursive subdivider call
  std::array<std::uint32_t, 4> whole;     // release of a child drawn whole
  std::array<std::uint32_t, 4> culled;    // release of a culled child
  std::array<std::uint32_t, 4> band;      // release of a child in the edge-fan layout
  std::uint32_t spare;                    // release of the fan's unused fourth child (triangles only)
};

struct SplitFormat {
  PacketKind kind;
  void (*subdivide)(MeshCpu &);
  std::uint32_t stride;      // bytes between a packet's corner records (colour, xy and, textured, uv)
  std::uint32_t quadCommand; // link word's length byte for a quad and for a triangle
  std::uint32_t triangleCommand;
  SplitSites sites;

  bool textured() const {
    return kind == PacketKind::Textured;
  }
  std::uint32_t colour(unsigned corner) const {
    return 4u + stride * corner;
  }
  std::uint32_t xy(unsigned corner) const {
    return 8u + stride * corner;
  }
  std::uint32_t uv(unsigned corner) const {
    return 12u + stride * corner;
  }
};

} // namespace ts2
