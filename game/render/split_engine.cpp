#include "render/split_engine.h"

#include "render/slot_packets.h"
#include "render/static_mesh_layout.h"

namespace ts2 {
namespace {

using namespace mesh_scratch;
using namespace static_scratch;

constexpr unsigned kChildFlag = 39;
constexpr unsigned kUvFlag = 7;
constexpr std::uint32_t kSwapped = 8;
constexpr unsigned kLodLevel = 38;

} // namespace

std::uint32_t &SplitEngine::child(unsigned i) {
  switch (i) {
  case 0:
    return c_.t0;
  case 1:
    return c_.t1;
  case 2:
    return c_.t2;
  default:
    return c_.t3;
  }
}

void SplitEngine::saveFrame() {
  MeshCpu &c = c_;
  const std::array<std::uint32_t, 4> depths{c.t0, c.t1, c.t2, c.t3};
  const std::array<std::uint32_t, 4> sxy{c.t4, c.t5, c.t6, c.t7};
  for (unsigned i = 0; i != corners_; ++i) {
    c.sh(c.t9 + points_[i].depth, depths[i]);
    c.sw(c.t9 + points_[i].sxy, sxy[i]);
  }
  const std::array<std::uint32_t, 6> saved{c.a0, c.a1, c.s0, c.s2, c.s5, c.s6};
  for (unsigned i = 0; i != saved.size(); ++i) {
    c.sw(c.t9 + split_frame::kSaved + 4u * i, saved[i]);
  }
}

void SplitEngine::leave() {
  MeshCpu &c = c_;
  c.a0 = c.lw(c.t9 + split_frame::kSaved + 0);
  c.a1 = c.lw(c.t9 + split_frame::kSaved + 4);
  c.s0 = c.lw(c.t9 + split_frame::kSaved + 8);
  c.s2 = c.lw(c.t9 + split_frame::kSaved + 12);
  c.s5 = c.lw(c.t9 + split_frame::kSaved + 16);
  c.s6 = c.lw(c.t9 + split_frame::kSaved + 20);
}

void SplitEngine::release(std::uint32_t site) {
  c_.ra = site;
  releaseSubtree(c_, f_.kind);
}

// The eight or six tested points are the middles the two projections give and the corners. The x byte and the y
// byte each hold a bit per point, the first point in the top bit. A coordinate the centre already shows on
// screen counts for all points; else a point counts when it lies on the same side as the centre.
void SplitEngine::storeScreenMask(std::uint32_t flags,
                                  std::uint32_t xCentre,
                                  std::uint32_t yCentre,
                                  std::span<const std::uint32_t> points) {
  constexpr std::uint32_t kXLimit = 0x02000000u; // 512 columns, in the top half of a word
  constexpr std::uint32_t kYLimit = 0x00F00000u; // 240 rows
  std::uint32_t xs = 0xFFu;
  if ((flags & 2u) == 0u) {
    const bool left = negative(xCentre << 16);
    xs = 0;
    for (const std::uint32_t p : points) {
      const bool on = left ? !negative(p << 16) : MeshCpu::slt(p << 16, kXLimit) != 0u;
      xs = (xs << 1) | (on ? 1u : 0u);
    }
  }
  std::uint32_t ys = 0xFFu;
  if ((flags & 1u) == 0u) {
    const bool above = negative(yCentre);
    ys = 0;
    for (const std::uint32_t p : points) {
      const bool on = above ? !negative(p) : MeshCpu::slt(p, kYLimit) != 0u;
      ys = (ys << 1) | (on ? 1u : 0u);
    }
  }
  c_.sw(c_.t9 + split_frame::kMask, (xs << 8) | ys);
}

bool SplitEngine::visible(std::uint32_t mask) const {
  const std::uint32_t seen = c_.lw(c_.t9 + split_frame::kMask);
  return (seen & mask) != 0u && ((seen >> 8) & mask) != 0u;
}

std::uint32_t SplitEngine::depth(unsigned point) const {
  const PointSlot &slot = points_[point];
  return slot.corner ? c_.lh(c_.t9 + slot.depth) : c_.lw(c_.t9 + slot.depth);
}

std::uint32_t SplitEngine::vertexOf(unsigned point) const {
  const PointSlot &slot = points_[point];
  return slot.corner ? c_.lw(c_.t9 + slot.vertex) : c_.t9 + slot.vertex;
}

bool SplitEngine::fan() const {
  return fan_;
}

bool SplitEngine::settleChildren() {
  MeshCpu &c = c_;
  c.s6 = c.lb(c.a3 + kChildFlag);
  c.at = c.lw(kAltPackets);
  if (c.s6 != 0u) {
    adoptChildren();
    return true;
  }
  c.s0 = c.lw(kFree);
  c.t0 = c.lhu(c.s0 + 0);
  c.t1 = c.lhu(c.s0 + 2);
  c.t2 = c.lhu(c.s0 + 4);
  c.t3 = c.lhu(c.s0 + 6);
  if (c.t0 == 0u || c.t1 == 0u || c.t2 == 0u || c.t3 == 0u) {
    return false;
  }
  c.s2 = (c.t1 << 16) + c.t0;
  c.s5 = (c.t3 << 16) + c.t2;
  c.t4 = (c.a3 - c.s3) + c.at;
  c.sw(c.a3 + 8, c.s2);
  c.sw(c.a3 + 8 + f_.stride, c.s5);
  for (unsigned i = 0; i != 4; ++i) {
    child(i) <<= 2;
  }
  c.s0 += 8;
  c.sw(kFree, c.s0);
  c.v1 = MeshCpu::slt(c.v0, fanBelow_);
  fan_ = c.v1 != 0u;
  c.t5 = fan_ ? 2u : 1u;
  c.sb(c.a3 + kChildFlag, c.t5);
  c.t5 = 0 - c.t5;
  c.sb(c.t4 + kChildFlag, c.t5);
  std::array<std::uint32_t, 4> primary{};
  std::array<std::uint32_t, 4> alt{};
  for (unsigned i = 0; i != 4; ++i) {
    alt[i] = c.at + child(i);
    primary[i] = c.s3 + child(i);
  }
  c.t4 = alt[0];
  c.t5 = alt[1];
  c.t6 = alt[2];
  c.t7 = alt[3];
  for (unsigned i = 0; i != 4; ++i) {
    child(i) = primary[i];
  }
  for (unsigned i = 0; i != 4; ++i) {
    c.sh(primary[i] + kLodLevel, 0);
  }
  for (unsigned i = 0; i != 4; ++i) {
    c.sh(alt[i] + kLodLevel, 0);
  }
  splitAttributes(fan_ ? SplitLayout::Fan : SplitLayout::Quarters, SplitOrigin::Fresh, c.a3, primary, alt);
  return true;
}

// A primitive already split keeps its children, but a change of layout or of texture detail cuts them again.
void SplitEngine::adoptChildren() {
  MeshCpu &c = c_;
  if (negative(c.s6)) {
    c.t4 = (c.a3 - c.s3) + c.at;
    c.t0 = c.lw(c.t4 + 8);
    c.t2 = c.lw(c.t4 + 8 + f_.stride);
    c.sw(c.a3 + 8, c.t0);
    c.sw(c.a3 + 8 + f_.stride, c.t2);
    c.s6 = 0 - c.s6;
    c.sb(c.a3 + kChildFlag, c.s6);
  } else {
    c.t0 = c.lw(c.a3 + 8);
    c.t2 = c.lw(c.a3 + 8 + f_.stride);
  }
  c.s6 -= 1;
  c.t1 = c.t0 >> 16;
  c.t3 = c.t2 >> 16;
  c.t0 &= 0xFFFFu;
  c.t2 &= 0xFFFFu;
  for (unsigned i = 0; i != 4; ++i) {
    child(i) = (child(i) << 2) + c.s3;
  }
  c.v1 = MeshCpu::slt(c.v0, fanBelow_);
  c.a0 = c.lb(kLodChanged);
  fan_ = c.v1 != 0u;
  const bool wrongLayout = fan_ ? c.s6 == 0u : c.sgn(c.s6) > 0;
  if (!wrongLayout && c.sgn(c.a0) <= 0) {
    return;
  }
  c.sb(c.a3 + kChildFlag, fan_ ? 2u : 1u);
  const std::array<std::uint32_t, 4> primary{c.t0, c.t1, c.t2, c.t3};
  splitAttributes(fan_ ? SplitLayout::Fan : SplitLayout::Quarters, SplitOrigin::Adopted, c.a3, primary, {});
}

// A culled quarter hands back whatever it held.
void SplitEngine::cullQuarter(unsigned i) {
  MeshCpu &c = c_;
  c.t4 = c.lb(child(i) + kChildFlag);
  c.t5 = c.ra;
  c.a3 = child(i);
  if (c.t4 != 0u) {
    release(f_.sites.culled[i]);
    c.ra = c.t5;
  }
}

// Calls the subdivider on the child as a primitive of its own, in a new frame. True when it handled the child.
bool SplitEngine::subdivideChild(unsigned i, std::span<const unsigned> ring) {
  MeshCpu &c = c_;
  c.s0 = c.t0;
  c.s2 = c.t1;
  c.s5 = c.t2;
  c.s6 = c.t3;
  c.a3 = child(i);
  std::array<std::uint32_t, 4> vertices{};
  std::array<std::uint32_t, 4> sxy{};
  for (unsigned k = 0; k != ring.size(); ++k) {
    vertices[k] = vertexOf(ring[k]);
    sxy[k] = c.lw(c.t9 + points_[ring[k]].sxy);
  }
  c.t4 = sxy[0];
  c.t5 = sxy[1];
  c.t6 = sxy[2];
  if (ring.size() == 4) {
    c.t7 = sxy[3];
  }
  c.t0 = vertices[0];
  c.t1 = vertices[1];
  c.t2 = vertices[2];
  if (ring.size() == 4) {
    c.t3 = vertices[3];
  }
  c.t9 += split_frame::kSize;
  for (unsigned k = 0; k != ring.size(); ++k) {
    c.sw(c.t9 + split_frame::kVertices + 4u * k, vertices[k]);
  }
  c.ra = f_.sites.subdivide[i];
  f_.subdivide(c);
  c.t9 -= split_frame::kSize;
  c.t0 = c.s0;
  c.t1 = c.s2;
  c.t2 = c.s5;
  c.t3 = c.s6;
  return c.v0 != 0u;
}

void SplitEngine::releaseWhole(unsigned i) {
  MeshCpu &c = c_;
  c.t4 = c.lb(child(i) + kChildFlag);
  c.t5 = c.ra;
  c.a3 = child(i);
  if (c.t4 != 0u) {
    release(f_.sites.whole[i]);
    c.ra = c.t5;
  }
}

// Puts a child on the ordering table at `bucket`, the address of the offset its depth selects, and fills in its
// corners.
void SplitEngine::link(std::uint32_t packet,
                       std::uint32_t bucket,
                       std::uint32_t command,
                       std::span<const unsigned> corners) {
  MeshCpu &c = c_;
  c.at = bucket;
  c.t4 = command;
  c.t5 = c.lhu(c.at + 0);
  c.t6 = packet << 8;
  c.at = c.t5 + c.a2;
  c.v1 = c.lw(c.at + 0);
  c.t6 >>= 8;
  c.v1 += c.t4;
  c.sw(c.at + 0, c.t6);
  c.sw(packet + 0, c.v1);
  for (unsigned k = 0; k != corners.size(); ++k) {
    c.sw(packet + f_.xy(k), c.lw(c.t9 + points_[corners[k]].sxy));
  }
  bindPacket(c.memory(), packet);
}

void SplitEngine::releaseSpare() {
  MeshCpu &c = c_;
  c.t4 = c.lb(c.t3 + kChildFlag);
  c.t5 = c.ra;
  c.a3 = c.t3;
  if (c.t4 != 0u) {
    release(f_.sites.spare);
    c.ra = c.t5;
  }
}

void SplitEngine::swapLastCorners(std::uint32_t packet) {
  MeshCpu &c = c_;
  c.t6 = c.lw(packet + f_.colour(2));
  c.t7 = c.lw(packet + f_.colour(3));
  c.sw(packet + f_.colour(3), c.t6);
  c.sw(packet + f_.colour(2), c.t7);
  if (f_.textured()) {
    c.t6 = c.lh(packet + f_.uv(2));
    c.t7 = c.lh(packet + f_.uv(3));
    c.sh(packet + f_.uv(3), c.t6);
    c.sh(packet + f_.uv(2), c.t7);
  }
}

// A fan child is a quad when its edge was cut short by a neighbour, which keeps the edge's middle as a corner,
// else a triangle; the packet's last two corners swap places when the form changes.
void SplitEngine::band(unsigned i, const Band &b) {
  MeshCpu &c = c_;
  const std::uint32_t packet = child(i);
  if (!visible(b.mask)) {
    return;
  }
  c.at = depth(b.depths[0]) + depth(b.depths[1]);
  c.at += depth(b.depths[2]);
  const std::uint32_t sum = c.at + depth(b.depths[3]);
  c.t4 = c.lb(packet + kChildFlag);
  c.t5 = c.ra;
  c.a3 = packet;
  if (c.t4 != 0u) {
    c.s0 = c.v0;
    release(f_.sites.band[i]);
    c.v0 = c.s0;
    c.ra = c.t5;
  }
  c.t6 = c.v0 & b.edge;
  const bool isQuad = c.t6 != 0u;
  c.t6 = c.lb(packet + kUvFlag);
  const bool swapped = (c.t6 & kSwapped) != 0u;
  c.t7 = c.t6 & kSwapped;
  c.t6 = isQuad ? (c.t6 | kSwapped) : (c.t6 & 0xFFF7u);
  if (swapped != isQuad) {
    c.sb(packet + kUvFlag, c.t6);
    swapLastCorners(packet);
  }
  const std::uint32_t bucket = ((sum >> 2) & 0xFFFCu) + c.s1;
  if (isQuad) {
    link(packet, bucket, f_.quadCommand, b.quad);
  } else {
    link(packet, bucket, f_.triangleCommand, b.triangle);
  }
}

} // namespace ts2
