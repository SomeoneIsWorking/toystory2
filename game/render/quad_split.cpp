#include "render/quad_split.h"

#include "gte_registers.h"
#include "render/mesh_subdividers.h"
#include "render/quad_split_attributes.h"
#include "render/split_engine.h"
#include "render/static_mesh_layout.h"

#include <array>
#include <span>

namespace ts2 {
namespace {

using namespace mesh_scratch;

// The nine points of a split quad: its corners in ring order and the middles of its edges and of the diagonal.
enum Point : unsigned { C0, C1, C2, C3, M01, M12, M23, M30, X };

constexpr std::array<PointSlot, 9> kPoints{{
    {110, 64, 0, true},
    {118, 68, 4, true},
    {126, 72, 8, true},
    {134, 76, 12, true},
    {44, 80, 104, false},
    {48, 84, 112, false},
    {52, 88, 120, false},
    {56, 92, 128, false},
    {60, 96, 136, false},
}};

// A quarter of the quad: its ring, and the bits of the point mask it needs on screen. Children come in the
// packet's corner order (C0, C1, C3, C2).
struct Quarter {
  std::array<unsigned, 4> ring;
  std::uint32_t mask;
};
constexpr std::array<Quarter, 4> kQuarters{{
    {{C0, M01, X, M30}, 0xC8u},
    {{M01, C1, M12, X}, 0xA4u},
    {{M30, X, M23, C3}, 0x51u},
    {{X, M12, C2, M23}, 0x32u},
}};

constexpr std::array<Band, 4> kBands{{
    {{C0, X, C1, M01}, {C0, X, M01, C1}, {C0, X, C1}, 0x8Cu, 8u},
    {{C1, X, C2, M12}, {C1, X, M12, C2}, {C1, X, C2}, 0x26u, 2u},
    {{C3, X, C0, M30}, {C3, X, M30, C0}, {C3, X, C0}, 0x49u, 4u},
    {{C2, X, C3, M23}, {C2, X, M23, C3}, {C2, X, C3}, 0x13u, 1u},
}};

constexpr unsigned kFanBelow = 15; // fewer short edges than all four cut the quad as a fan
constexpr std::uint32_t kFullMask = 0xFFFFu;
constexpr std::uint32_t kYLimit = 0x00F00000u;

constexpr SplitFormat kTexturedQuad{PacketKind::Textured,
                                    &subdivideQuadTextured,
                                    12,
                                    0x0C000000u,
                                    0x09000000u,
                                    {{0x800150E8u, 0x80015234u, 0x80015380u, 0x800154CCu},
                                     {0x80015134u, 0x80015280u, 0x800153CCu, 0x80015518u},
                                     {0x800151A4u, 0x800152F0u, 0x8001543Cu, 0x80015588u},
                                     {0x80015610u, 0x80015764u, 0x800158B8u, 0x80015A0Cu},
                                     0}};
constexpr SplitFormat kPlainQuad{PacketKind::Plain,
                                 &subdivideQuadPlain,
                                 8,
                                 0x08000000u,
                                 0x06000000u,
                                 {{0x8001A880u, 0x8001A9CCu, 0x8001AB18u, 0x8001AC64u},
                                  {0x8001A8CCu, 0x8001AA18u, 0x8001AB64u, 0x8001ACB0u},
                                  {0x8001A93Cu, 0x8001AA88u, 0x8001ABD4u, 0x8001AD20u},
                                  {0x8001ADA8u, 0x8001AEDCu, 0x8001B010u, 0x8001B144u},
                                  0}};

class QuadSplit final : public SplitEngine {
public:
  QuadSplit(MeshCpu &c, const SplitFormat &format) : SplitEngine(c, format, kPoints, 4, kFanBelow) {}

  void run();

private:
  void splitAttributes(SplitLayout layout,
                       SplitOrigin origin,
                       std::uint32_t parent,
                       std::span<const std::uint32_t> primary,
                       std::span<const std::uint32_t> alt) override {
    splitQuadAttributes(c_, f_, layout, origin, parent, primary, alt);
  }

  void projectMiddles();
  void quarters();
  void quarter(unsigned i);
  void drawWhole(unsigned i, const Quarter &q);
  void bands();
};

// The middle of two packed xy words, each coordinate halved towards zero as the guest's mask-shift-restore does.
std::uint32_t middleXy(std::uint32_t a, std::uint32_t b) {
  const std::uint32_t sum = a + b;
  return MeshCpu::sra(sum & 0xFFFEFFFEu, 1) + (sum & 0x8000u);
}

// Projects the five middles in two RTPTs (the diagonal's middle in both) and stores their vertices, depths and
// screen positions in the frame, with the mask of which tested points are on screen.
void QuadSplit::projectMiddles() {
  MeshCpu &c = c_;
  c.t0 = c.lw(c.t9 + 0);
  c.t1 = c.lw(c.t9 + 4);
  c.t2 = c.lw(c.t9 + 8);
  c.t3 = c.lw(c.t9 + 12);
  c.a0 = c.lh(c.t0 + 4);
  c.s0 = c.lh(c.t1 + 4);
  c.s2 = c.lh(c.t2 + 4);
  c.s6 = c.lh(c.t3 + 4);
  const auto middleZ = [&](std::uint32_t a, std::uint32_t b) {
    return MeshCpu::sra(a + b, 1);
  };
  c.v1 = middleZ(c.a0, c.s0);
  c.sh(c.t9 + 108, c.v1);
  gteWrite(1, c.v1);
  c.v1 = middleZ(c.s6, c.a0);
  c.sh(c.t9 + 132, c.v1);
  gteWrite(3, c.v1);
  c.v1 = middleZ(c.s0, c.s6);
  c.sw(c.t9 + 140, c.v1);
  gteWrite(5, c.v1);
  c.v1 = middleZ(c.s0, c.s2);
  c.sh(c.t9 + 116, c.v1);
  c.v1 = middleZ(c.s2, c.s6);
  c.sh(c.t9 + 124, c.v1);
  c.s5 = 0xFFFEFFFEu;
  c.a0 = c.lw(c.t0 + 0) & c.s5;
  c.s0 = c.lw(c.t1 + 0) & c.s5;
  c.s2 = c.lw(c.t2 + 0) & c.s5;
  c.s6 = c.lw(c.t3 + 0) & c.s5;
  c.v1 = middleXy(c.a0, c.s0);
  c.sw(c.t9 + 104, c.v1);
  gteWrite(0, c.v1);
  c.v1 = middleXy(c.s6, c.a0);
  c.sw(c.t9 + 128, c.v1);
  gteWrite(2, c.v1);
  c.v1 = middleXy(c.s0, c.s6);
  c.sw(c.t9 + 136, c.v1);
  gteWrite(4, c.v1);
  c.v1 = middleXy(c.s0, c.s2);
  c.gteOp(0x4A280030u);
  c.sw(c.t9 + 112, c.v1);
  c.a0 = middleXy(c.s2, c.s6);
  c.sw(c.t9 + 120, c.a0);
  c.t0 = gteRead(12);
  c.t1 = gteRead(13);
  c.t2 = gteRead(14);
  c.sw(c.t9 + 44, gteRead(17));
  c.sw(c.t9 + 56, gteRead(18));
  c.sw(c.t9 + 60, gteRead(19));
  gteWrite(0, c.v1);
  gteWrite(2, c.a0);
  gteWrite(1, c.lw(c.t9 + 116));
  gteWrite(3, c.lw(c.t9 + 124));
  c.gteOp(0x4A280030u);
  c.sw(c.t9 + 80, c.t0);
  c.sw(c.t9 + 92, c.t1);
  c.sw(c.t9 + 96, c.t2);
  c.s0 = ((c.t2 & 0xFFFFu) < 512u ? 2u : 0u) | (c.t2 < kYLimit ? 1u : 0u);
  const std::uint32_t second0 = gteRead(12);
  const std::uint32_t second1 = gteRead(13);
  c.sw(c.t9 + 48, gteRead(17));
  c.sw(c.t9 + 52, gteRead(18));
  c.sw(c.t9 + 84, second0);
  c.sw(c.t9 + 88, second1);
  if (c.s0 == 3u) {
    c.sw(c.t9 + split_frame::kMask, kFullMask);
    return;
  }
  const std::array<std::uint32_t, 8> points{c.t0, c.t1, second0, second1, c.t4, c.t5, c.t6, c.t7};
  storeScreenMask(c.s0, c.t2, c.t2, points);
}

// Quarters: each child is culled, subdivided again if it is near enough, or linked whole.
void QuadSplit::quarters() {
  MeshCpu &c = c_;
  c.sh(c.t9 + split_frame::kNear, c.a1);
  c.sh(c.t9 + split_frame::kNear + 2, c.s4);
  c.a1 = MeshCpu::sra(c.a1, 2);
  c.s4 = MeshCpu::sra(c.s4, 2);
  c.a0 = c.ra;
  for (unsigned i = 0; i != 4; ++i) {
    quarter(i);
  }
  c.ra = c.a0;
  c.v0 = 1;
  c.a1 = c.lh(c.t9 + split_frame::kNear);
  c.s4 = c.lh(c.t9 + split_frame::kNear + 2);
}

void QuadSplit::quarter(unsigned i) {
  MeshCpu &c = c_;
  const Quarter &q = kQuarters[i];
  if (!visible(q.mask)) {
    cullQuarter(i);
    return;
  }
  c.v1 = depth(q.ring[0]) - c.s4;
  if (negative(c.v1)) {
    gteWrite(19, depth(q.ring[0]));
    gteWrite(18, depth(q.ring[1]));
    gteWrite(16, depth(q.ring[2]));
    gteWrite(17, depth(q.ring[3]));
    if (subdivideChild(i, q.ring)) {
      return;
    }
  }
  drawWhole(i, q);
}

void QuadSplit::drawWhole(unsigned i, const Quarter &q) {
  MeshCpu &c = c_;
  const std::uint32_t packet = child(i);
  c.at = depth(q.ring[0]) + depth(q.ring[1]);
  c.at += depth(q.ring[2]);
  const std::uint32_t sum = c.at + depth(q.ring[3]);
  releaseWhole(i);
  const std::array<unsigned, 4> corners{q.ring[0], q.ring[1], q.ring[3], q.ring[2]};
  link(packet, ((sum >> 2) & 0xFFFCu) + c.s1, f_.quadCommand, corners);
}

// The fan: each child sits on one edge of the quad.
void QuadSplit::bands() {
  MeshCpu &c = c_;
  c.a0 = c.ra;
  for (unsigned i = 0; i != 4; ++i) {
    band(i, kBands[i]);
  }
  c.ra = c.a0;
  c.v0 = 1;
}

void QuadSplit::run() {
  saveFrame();
  projectMiddles();
  if (!settleChildren()) {
    leave();
    return;
  }
  if (fan()) {
    bands();
  } else {
    quarters();
  }
  leave();
}

} // namespace

void splitQuad(MeshCpu &c, PacketKind kind) {
  QuadSplit(c, kind == PacketKind::Textured ? kTexturedQuad : kPlainQuad).run();
}

} // namespace ts2
