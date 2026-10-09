#include "render/mesh_subdividers.h"

#include "render/quad_split.h"
#include "render/slot_release.h"
#include "render/tri_split.h"
#include <lucent/log.h>

namespace ts2 {
namespace {

constexpr std::uint32_t kMaxDepthFrame = 648u;
constexpr std::uint32_t kNearEnough = 50u;

// Edges of the quad whose summed depths fall short of the threshold, one bit each. Returns the bits; the
// guest leaves the last sum in v1 and the depths restored in t0 and t2.
std::uint32_t shortEdgesOfQuad(MeshCpu &c) {
  c.t1 = gteRead(kSz2);
  c.t2 = gteRead(kSz0);
  c.t3 = gteRead(psx::gte::kSz1);
  c.t0 -= c.a1;
  c.t2 -= c.a1;
  c.v0 = 0;
  c.v1 = c.t1 + c.t0;
  bool edge = negative(c.v1);
  c.v1 = c.t3 + c.t0;
  c.v0 += edge ? 8u : 0u;
  edge = negative(c.v1);
  c.v1 = c.t1 + c.t2;
  c.v0 += edge ? 4u : 0u;
  edge = negative(c.v1);
  c.v1 = c.t3 + c.t2;
  c.v0 += edge ? 2u : 0u;
  edge = negative(c.v1);
  c.t0 += c.a1;
  c.v0 += edge ? 1u : 0u;
  c.t2 += c.a1;
  return c.v0;
}

// The same for a triangle's three edges (4, 2, 1); t3 is the caller's.
std::uint32_t shortEdgesOfTriangle(MeshCpu &c) {
  c.t2 = gteRead(psx::gte::kSz1);
  c.t1 = gteRead(kSz2);
  c.t0 = gteRead(kSz3);
  c.v0 = 0;
  c.v1 = c.t1 + c.t0;
  c.v1 -= c.a1;
  bool edge = negative(c.v1);
  c.v1 = c.t2 + c.t0;
  c.v0 += edge ? 4u : 0u;
  c.v1 -= c.a1;
  edge = negative(c.v1);
  c.v1 = c.t1 + c.t2;
  c.v0 += edge ? 2u : 0u;
  c.v1 -= c.a1;
  edge = negative(c.v1);
  c.v1 = c.t3 + c.t2;
  c.v0 += edge ? 1u : 0u;
  return c.v0;
}

// A primitive whose every depth is under the limit is not drawn; a subdivided one has its children released.
// Returns false at the first depth that is not, leaving v1 as the guest does.
bool allNearer(MeshCpu &c, std::initializer_list<std::uint32_t> depths) {
  for (const std::uint32_t depth : depths) {
    c.v1 = c.slt(depth, kNearEnough);
    if (c.v1 == 0u) {
      return false;
    }
  }
  return true;
}

} // namespace

void subdivideQuadTextured(MeshCpu &c) {
  c.v1 = (c.t9 & 0xFFFFu) - kMaxDepthFrame;
  c.t0 = gteRead(kSz3);
  if (c.sgn(c.v1) > 0) {
    c.v0 = 0;
    return;
  }
  if (shortEdgesOfQuad(c) == 0u) {
    return;
  }
  if (!allNearer(c, {c.t0, c.t1, c.t2, c.t3})) {
    splitQuad(c, PacketKind::Textured);
    return;
  }
  c.v1 = c.lb(c.a3 + 39u);
  c.t5 = c.ra;
  if (c.v1 == 0u) {
    return;
  }
  c.t6 = c.a3;
  c.ra = 0x80014DE4u;
  releaseSubtree(c, PacketKind::Textured);
  c.ra = c.t5;
  c.a3 = c.t6;
  c.v0 = 15u;
}

void subdivideTriangleTextured(MeshCpu &c) {
  c.v1 = (c.t9 & 0xFFFFu) - kMaxDepthFrame;
  c.t2 = gteRead(psx::gte::kSz1);
  if (c.sgn(c.v1) > 0) {
    c.v0 = 0;
    return;
  }
  if (shortEdgesOfTriangle(c) == 0u) {
    c.v0 = 0;
    return;
  }
  if (!allNearer(c, {c.t0, c.t1, c.t2})) {
    splitTriangle(c, PacketKind::Textured);
    return;
  }
  c.v1 = c.lb(c.a3 + 39u);
  c.t5 = c.ra;
  if (c.v1 == 0u) {
    return;
  }
  c.t6 = c.a3;
  c.ra = 0x8001672Cu;
  releaseSubtree(c, PacketKind::Textured);
  c.ra = c.t5;
  c.a3 = c.t6;
  c.v0 = 15u;
}

void subdivideQuadPlain(MeshCpu &c) {
  c.v1 = (c.t9 & 0xFFFFu) - kMaxDepthFrame;
  c.t0 = gteRead(kSz3);
  if (c.sgn(c.v1) > 0) {
    c.v0 = 0;
    return;
  }
  if (shortEdgesOfQuad(c) == 0u) {
    return;
  }
  if (!allNearer(c, {c.t0, c.t1, c.t2, c.t3})) {
    splitQuad(c, PacketKind::Plain);
    return;
  }
  c.v1 = c.lb(c.a3 + 39u);
  c.t5 = c.ra;
  if (c.v1 == 0u) {
    return;
  }
  c.t6 = c.a3;
  c.ra = 0x8001A57Cu;
  releaseSubtree(c, PacketKind::Plain);
  c.ra = c.t5;
  c.a3 = c.t6;
  c.v0 = 15u;
}

void subdivideTrianglePlain(MeshCpu &c) {
  c.v1 = (c.t9 & 0xFFFFu) - kMaxDepthFrame;
  c.t2 = gteRead(psx::gte::kSz1);
  if (c.sgn(c.v1) > 0) {
    c.v0 = 0;
    return;
  }
  if (shortEdgesOfTriangle(c) == 0u) {
    c.v0 = 0;
    return;
  }
  if (!allNearer(c, {c.t0, c.t1, c.t2})) {
    splitTriangle(c, PacketKind::Plain);
    return;
  }
  c.v1 = c.lb(c.a3 + 39u);
  c.t5 = c.ra;
  if (c.v1 == 0u) {
    return;
  }
  c.t6 = c.a3;
  c.ra = 0x8001B948u;
  releaseSubtree(c, PacketKind::Plain);
  c.ra = c.t5;
  c.a3 = c.t6;
  c.v0 = 15u;
}

} // namespace ts2
