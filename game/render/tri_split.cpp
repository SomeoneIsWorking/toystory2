#include "render/tri_split.h"

#include "gte_registers.h"
#include "render/mesh_subdividers.h"
#include "render/split_engine.h"
#include "render/static_mesh_layout.h"
#include "render/tri_split_attributes.h"

#include <array>
#include <span>

namespace ts2 {
namespace {

// The seven points of a split triangle: its corners, the middles of its edges and the middle of the first
// edge's middle and the third corner.
enum Point : unsigned { C0, C1, C2, M01, M12, M20, X };

constexpr std::array<PointSlot, 7> kPoints{{
    {110, 64, 0, true},
    {118, 68, 4, true},
    {126, 72, 8, true},
    {44, 80, 104, false},
    {48, 84, 112, false},
    {52, 88, 120, false},
    {60, 96, 136, false},
}};

struct Quarter {
  std::array<unsigned, 3> ring;
  std::uint32_t mask;
};
constexpr std::array<Quarter, 4> kQuarters{{
    {{C0, M01, M20}, 0x34u},
    {{M01, C1, M12}, 0x2Au},
    {{M20, M12, C2}, 0x19u},
    {{M01, M12, M20}, 0x38u},
}};

constexpr std::array<Band, 3> kBands{{
    {{C0, X, C1, M01}, {C0, X, M01, C1}, {C0, X, C1}, 0x26u, 4u},
    {{C1, X, C2, M12}, {C1, X, M12, C2}, {C1, X, C2}, 0x0Bu, 1u},
    {{C2, X, C0, M20}, {C2, X, M20, C0}, {C2, X, C0}, 0x15u, 2u},
}};

constexpr unsigned kFanBelow = 7; // fewer short edges than all three cut the triangle as a fan
constexpr std::uint32_t kFullMask = 0xFFFFu;
constexpr std::uint32_t kYLimit = 0x00F00000u;
constexpr std::uint32_t kAverageDepths = 0x4B58002Du; // AVSZ3

constexpr SplitFormat kTexturedTriangle{PacketKind::Textured,
                                        &subdivideTriangleTextured,
                                        12,
                                        0x0C000000u,
                                        0x09000000u,
                                        {{0x800169CCu, 0x80016AE4u, 0x80016BFCu, 0x80016D14u},
                                         {0x80016A00u, 0x80016B18u, 0x80016C30u, 0x80016D48u},
                                         {0x80016A68u, 0x80016B80u, 0x80016C98u, 0x80016DB0u},
                                         {0x80016E50u, 0x80016FA4u, 0x800170F8u, 0},
                                         0x80016E00u}};
constexpr SplitFormat kPlainTriangle{PacketKind::Plain,
                                     &subdivideTrianglePlain,
                                     8,
                                     0x08000000u,
                                     0x06000000u,
                                     {{0x8001BBE8u, 0x8001BD00u, 0x8001BE18u, 0x8001BF30u},
                                      {0x8001BC1Cu, 0x8001BD34u, 0x8001BE4Cu, 0x8001BF64u},
                                      {0x8001BC84u, 0x8001BD9Cu, 0x8001BEB4u, 0x8001BFCCu},
                                      {0x8001C06Cu, 0x8001C1A0u, 0x8001C2D4u, 0},
                                      0x8001C01Cu}};

class TriangleSplit final : public SplitEngine {
public:
  TriangleSplit(MeshCpu &c, const SplitFormat &format) : SplitEngine(c, format, kPoints, 3, kFanBelow) {}

  void run();

private:
  void splitAttributes(SplitLayout layout,
                       SplitOrigin,
                       std::uint32_t parent,
                       std::span<const std::uint32_t> primary,
                       std::span<const std::uint32_t> alt) override {
    splitTriangleAttributes(c_, f_, layout, parent, primary, alt);
  }

  void projectMiddles();
  void quarters();
  void quarter(unsigned i);
  void drawWhole(unsigned i, const Quarter &q);
  void bands();
};

std::uint32_t middleXy(std::uint32_t a, std::uint32_t b) {
  const std::uint32_t sum = a + b;
  return MeshCpu::sra(sum & 0xFFFEFFFEu, 1) + (sum & 0x8000u);
}

// Projects the middles: three in an RTPT, the fourth in an RTPS, and stores their vertices, depths and screen
// positions in the frame, with the mask of which tested points are on screen.
void TriangleSplit::projectMiddles() {
  MeshCpu &c = c_;
  c.t0 = c.lw(c.t9 + 0);
  c.t1 = c.lw(c.t9 + 4);
  c.t2 = c.lw(c.t9 + 8);
  c.a0 = c.lh(c.t0 + 4);
  c.s0 = c.lh(c.t1 + 4);
  c.s2 = c.lh(c.t2 + 4);
  c.v1 = MeshCpu::sra(c.a0 + c.s0, 1);
  c.sh(c.t9 + 108, c.v1);
  gteWrite(1, c.v1);
  c.v1 = MeshCpu::sra(c.v1 + c.s2, 1);
  c.sw(c.t9 + 140, c.v1);
  gteWrite(3, c.v1);
  c.v1 = MeshCpu::sra(c.s2 + c.a0, 1);
  c.sh(c.t9 + 124, c.v1);
  gteWrite(5, c.v1);
  c.v1 = MeshCpu::sra(c.s0 + c.s2, 1);
  c.sh(c.t9 + 116, c.v1);
  c.s5 = 0xFFFEFFFEu;
  c.a0 = c.lw(c.t0 + 0) & c.s5;
  c.s0 = c.lw(c.t1 + 0) & c.s5;
  c.s2 = c.lw(c.t2 + 0) & c.s5;
  c.v1 = middleXy(c.a0, c.s0);
  c.sw(c.t9 + 104, c.v1);
  gteWrite(0, c.v1);
  c.v1 = middleXy(c.v1 & c.s5, c.s2);
  c.sw(c.t9 + 136, c.v1);
  gteWrite(2, c.v1);
  c.v1 = middleXy(c.s2, c.a0);
  c.sw(c.t9 + 120, c.v1);
  gteWrite(4, c.v1);
  c.v1 = middleXy(c.s0, c.s2);
  c.gteOp(0x4A280030u);
  c.sw(c.t9 + 112, c.v1);
  c.t0 = gteRead(12);
  c.t1 = gteRead(13);
  c.t2 = gteRead(14);
  c.sw(c.t9 + 44, gteRead(17));
  c.sw(c.t9 + 60, gteRead(18));
  c.sw(c.t9 + 52, gteRead(19));
  gteWrite(0, c.v1);
  gteWrite(1, c.lw(c.t9 + 116));
  c.gteOp(0x4A180001u);
  c.sw(c.t9 + 80, c.t0);
  c.sw(c.t9 + 96, c.t1);
  c.sw(c.t9 + 88, c.t2);
  c.s0 = ((c.t1 & 0xFFFFu) < 512u ? 2u : 0u) | (c.t1 < kYLimit ? 1u : 0u);
  const std::uint32_t last = gteRead(14);
  c.sw(c.t9 + 48, gteRead(19));
  c.sw(c.t9 + 84, last);
  if (c.s0 == 3u) {
    c.sw(c.t9 + split_frame::kMask, kFullMask);
    return;
  }
  const std::array<std::uint32_t, 6> points{c.t0, c.t2, last, c.t4, c.t5, c.t6};
  storeScreenMask(c.s0, c.t1, c.t2, points);
}

void TriangleSplit::quarters() {
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

void TriangleSplit::quarter(unsigned i) {
  MeshCpu &c = c_;
  const Quarter &q = kQuarters[i];
  if (!visible(q.mask)) {
    cullQuarter(i);
    return;
  }
  gteWrite(19, depth(q.ring[0]));
  gteWrite(18, depth(q.ring[1]));
  c.v1 = depth(q.ring[0]) - c.s4;
  gteWrite(17, depth(q.ring[2]));
  if (negative(c.v1) && subdivideChild(i, q.ring)) {
    return;
  }
  drawWhole(i, q);
}

void TriangleSplit::drawWhole(unsigned i, const Quarter &q) {
  MeshCpu &c = c_;
  const std::uint32_t packet = child(i);
  c.gteOp(kAverageDepths);
  releaseWhole(i);
  c.at = gteRead(7);
  link(packet, (c.at << 2) + c.s1, f_.triangleCommand, q.ring);
}

// The fan: three children on the edges; the fourth, unused, holds nothing a past split left.
void TriangleSplit::bands() {
  MeshCpu &c = c_;
  c.a0 = c.ra;
  releaseSpare();
  for (unsigned i = 0; i != 3; ++i) {
    band(i, kBands[i]);
  }
  c.ra = c.a0;
  c.v0 = 1;
}

void TriangleSplit::run() {
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

void splitTriangle(MeshCpu &c, PacketKind kind) {
  TriangleSplit(c, kind == PacketKind::Textured ? kTexturedTriangle : kPlainTriangle).run();
}

} // namespace ts2
