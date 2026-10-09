#include "render/static_mesh_drawer.h"

#include "gte_registers.h"
#include "render/mesh_cpu.h"
#include "render/mesh_format.h"
#include "render/mesh_subdividers.h"
#include "render/slot_packets.h"
#include "render/slot_release.h"
#include "render/static_mesh_layout.h"

#include <array>

namespace ts2 {

using psx::present::EmitMemory;

namespace {

using namespace mesh_scratch;
using namespace static_scratch;

// The return addresses the guest's calls from a handler leave in ra: the subdivider, the release of a primitive
// drawn whole, and the release of a culled one.
struct CallSites {
  std::uint32_t subdivide;
  std::uint32_t whole;
  std::uint32_t culled;
};

struct Handler {
  PacketShape shape;
  bool noCull; // the quad is kept whatever way it faces
  CallSites sites;
  void (*subdivide)(MeshCpu &);
};

constexpr Handler kTexturedQuad{{true, true}, false, {0x80014C8Cu, 0x80014CACu, 0x80014D28u}, &subdivideQuadTextured};
constexpr Handler kTexturedQuadNoCull{
    {true, true}, true, {0x80014788u, 0x800147A8u, 0x80014824u}, &subdivideQuadTextured};
constexpr Handler kTexturedTriangle{
    {false, true}, false, {0x800142A8u, 0x800142C8u, 0x80014340u}, &subdivideTriangleTextured};
constexpr Handler kPlainQuad{{true, false}, false, {0x80010808u, 0x80010828u, 0x800108A4u}, &subdivideQuadPlain};
constexpr Handler kPlainTriangle{
    {false, false}, false, {0x80010EE0u, 0x80010F00u, 0x80010F78u}, &subdivideTrianglePlain};

// Opcodes 0 and 4 share a handler, as do 1 and 5, 2 and 6, 16 and 20, 17 and 21. The rest (3, 7, 8..15, 18, 19,
// 22, 23) are not reached by any replay and stay on the guest.
const Handler *handlerOf(std::uint32_t opcode) {
  switch (opcode) {
  case 0:
  case 4:
    return &kTexturedQuad;
  case 1:
  case 5:
    return &kTexturedTriangle;
  case 2:
  case 6:
    return &kTexturedQuadNoCull;
  case 16:
  case 20:
    return &kPlainQuad;
  case 17:
  case 21:
    return &kPlainTriangle;
  default:
    return nullptr;
  }
}

constexpr std::uint32_t kSubdivideNear = 3072u; // bucket offsets past these halve and quarter the texture UVs
constexpr std::uint32_t kSubdivideFar = 4224u;

class Walk {
public:
  Walk(const EmitMemory &memory, const MeshDrawCall &call) : m_(memory), call_(call), c_(memory) {}

  std::optional<MeshDrawResult> run();

private:
  enum class Outcome { Culled, Dropped, Handled, Linked };

  void enter();
  void selectTexture(std::uint32_t word);
  bool command(const Handler &handler);
  void stash(std::uint32_t word, bool quad) const;
  Outcome primitive(const Handler &handler, std::uint32_t stride);
  void projectQuad(const Handler &handler, std::uint32_t stride);
  void projectTriangle(std::uint32_t stride);
  bool culledQuad(const Handler &handler);
  bool culledTriangle();
  bool outOfDepth(bool quad);
  Outcome place(const Handler &handler);
  std::uint32_t fillLeftover(PacketShape shape, const std::array<std::uint32_t, 4> &corners) const;
  void resample(const Handler &handler);
  void rewriteUv(const Handler &handler, std::uint32_t level);
  void release(const Handler &handler, std::uint32_t site);
  Outcome cull(const Handler &handler);
  Outcome subdivideOrLink(const Handler &handler);

  const EmitMemory &m_;
  const MeshDrawCall &call_;
  MeshCpu c_;
};

void Walk::enter() {
  MeshCpu &c = c_;
  c.a0 = call_.args[0];
  c.a1 = call_.args[1];
  c.a2 = call_.args[2];
  c.a3 = call_.args[3];
  c.v1 = call_.v1;
  c.ra = call_.ra;
  c.s6 = call_.s6;
  c.sw(kNearLimit, c.lw(c.a3 + 12u));
  c.t7 = c.lw(c.a0);
  c.a0 += 4u;
  c.t9 = c.t7 << 3;
  c.s0 = c.a0 + c.t9;
  c.sw(kDrawn, 1);
  c.sw(kReturn, c.ra);
  c.s1 = c.lw(kBucketTables) + (c.a2 << 2);
  c.t8 = c.lw(kInstanceSlotTable);
  c.s5 = c.lw(kOtzLimit) << 2;
  c.s3 = c.lw(kSlotPacketBase);
  c.a2 = c.lw(kOrderingTable);
  c.a1 <<= 2;
  c.s4 = c.a1 + 1500u;
  c.a1 <<= 1;
}

// The command's texture word: the page words every textured packet reads, and whether the packet's UVs may be
// resampled by distance.
void Walk::selectTexture(std::uint32_t word) {
  MeshCpu &c = c_;
  const std::uint32_t select = setTexturePage(m_, static_cast<std::int32_t>(word));
  c.sh(kLodEnabled, select < 0x40u ? 1u : 0u);
  c.t3 = (c.lw(kTexturePages + select) & 0xFFFFu) + (word & 0x60u);
}

std::optional<MeshDrawResult> Walk::run() {
  MeshCpu &c = c_;
  enter();
  if (c.sgn(c.t7) <= 0) {
    return std::nullopt; // a transformed mesh: its header is not a positive vertex count
  }
  while (true) {
    c.t0 = c.lh(c.s0);
    c.t1 = c.t0 & 0x1Fu;
    if (!negative(c.t0)) {
      selectTexture(c.t0);
    }
    c.s2 = c.lh(c.s0 + 2u);
    c.s0 += 4u;
    if (c.t1 >= 24u) {
      break;
    }
    const Handler *handler = handlerOf(c.t1);
    if (handler == nullptr || !command(*handler)) {
      return std::nullopt;
    }
  }
  return MeshDrawResult{c.lw(kDrawn), c.v1};
}

// The vertex addresses of a descriptor's index bytes, kept where the next primitive reads them.
void Walk::stash(std::uint32_t word, bool quad) const {
  const MeshCpu &c = c_;
  c.sw(kCorners, ((word << 3) & 0x7F8u) + c.a0);
  c.sw(kCorners + 4u, ((word >> 5) & 0x7F8u) + c.a0);
  c.sw(kCorners + 8u, ((word >> 13) & 0x7F8u) + c.a0);
  if (quad) {
    c.sw(kCorners + 12u, ((word >> 21) & 0x7F8u) + c.a0);
  }
}

bool Walk::command(const Handler &handler) {
  MeshCpu &c = c_;
  const std::uint32_t stride = handler.shape.textured ? 12u : 4u;
  stash(c.lw(c.s0), handler.shape.quad);
  while (true) {
    const Outcome outcome = primitive(handler, stride);
    if (c.unported) {
      return false;
    }
    (void)outcome;
    c.t8 += 2u;
    const bool more = c.s2 != 0u;
    c.s0 += stride;
    if (!more) {
      return true;
    }
  }
}

void Walk::projectQuad(const Handler &handler, std::uint32_t stride) {
  MeshCpu &c = c_;
  c.t2 = c.lw(kCorners + 8u);
  c.t3 = c.lw(kCorners + 12u);
  gteWrite(psx::gte::kVxy0, c.lw(c.t2));
  gteWrite(psx::gte::kVz0, c.lw(c.t2 + 4u));
  c.s2 -= 1u;
  c.t1 = c.lw(kCorners + 4u);
  c.gteOp(psx::gte::kRtps);
  c.t6 = c.lw(c.t3);
  c.t7 = c.lh(c.t3 + 4u);
  c.t4 = c.lw(c.t1);
  c.a3 = c.lh(c.t1 + 4u);
  c.t9 = c.lh(c.t8);
  c.t0 = c.lw(kCorners);
  gteWrite(psx::gte::kVxy0, c.t6);
  gteWrite(psx::gte::kVz0, c.t7);
  gteWrite(psx::gte::kVxy1, c.t4);
  gteWrite(psx::gte::kVz1, c.a3);
  gteWrite(psx::gte::kVxy2, c.lw(c.t0));
  gteWrite(psx::gte::kVz2, c.lw(c.t0 + 4u));
  c.t6 = gteRead(psx::gte::kSxy2);
  c.v1 = c.lw(kRowMax);
  c.at = c.lw(kRowMin);
  c.gteOp(psx::gte::kRtpt);
  stash(c.lw(c.s0 + stride), true);
  c.sw(kVertices + 12u, c.t3);
  c.sw(kVertices + 8u, c.t2);
  c.t4 = gteRead(psx::gte::kSxy2);
  c.t5 = gteRead(psx::gte::kSxy1);
  c.t7 = gteRead(psx::gte::kSxy0);
  if (!handler.noCull) {
    c.gteOp(psx::gte::kNclip);
  }
}

void Walk::projectTriangle(std::uint32_t stride) {
  MeshCpu &c = c_;
  c.t0 = c.lw(kCorners);
  c.t1 = c.lw(kCorners + 4u);
  c.t2 = c.lw(kCorners + 8u);
  gteWrite(psx::gte::kVxy2, c.lw(c.t0));
  gteWrite(psx::gte::kVz2, c.lw(c.t0 + 4u));
  gteWrite(psx::gte::kVxy1, c.lw(c.t1));
  gteWrite(psx::gte::kVz1, c.lw(c.t1 + 4u));
  gteWrite(psx::gte::kVxy0, c.lw(c.t2));
  gteWrite(psx::gte::kVz0, c.lw(c.t2 + 4u));
  c.s2 -= 1u;
  c.gteOp(psx::gte::kRtpt);
  c.t9 = c.lh(c.t8);
  stash(c.lw(c.s0 + stride), false);
  c.sw(kVertices, c.t0);
  c.sw(kVertices + 4u, c.t1);
  c.sw(kVertices + 8u, c.t2);
  c.t4 = gteRead(psx::gte::kSxy2);
  c.t5 = gteRead(psx::gte::kSxy1);
  c.t6 = gteRead(psx::gte::kSxy0);
  c.gteOp(psx::gte::kNclip);
  c.at = c.lw(kRowMin);
  c.v1 = c.lw(kRowMax);
}

// Whether the quad lies off the screen or faces away; leaves the depth test's temporaries as the guest does.
bool Walk::culledQuad(const Handler &handler) {
  MeshCpu &c = c_;
  const std::array<std::uint32_t, 4> corners{c.t6, c.t4, c.t5, c.t7};
  if (rowsOutside(c, corners)) {
    return true;
  }
  c.at = c.lw(kColumnMin);
  c.v1 = c.lw(kColumnMax);
  if (columnsOutside(c, corners)) {
    // The branch that culls a quad whose first corner is left of the screen has the depth average in its delay
    // slot, which only the handler without the facing test starts with.
    if (negative(c.v0) && handler.noCull) {
      c.gteOp(psx::gte::kAvsz4);
    }
    return true;
  }
  if (handler.noCull) {
    c.gteOp(psx::gte::kAvsz4);
    c.a3 = c.lw(kNearLimit);
    c.at = gteRead(psx::gte::kOtz);
    return false;
  }
  c.v0 = gteRead(psx::gte::kMac0);
  c.gteOp(psx::gte::kAvsz4);
  c.a3 = c.lw(kNearLimit);
  gteWrite(psx::gte::kSxy2, c.t6);
  c.at = gteRead(psx::gte::kOtz);
  if (negative(c.v0)) {
    c.gteOp(psx::gte::kNclip);
    c.v1 = gteRead(psx::gte::kMac0);
    if (!negative(c.v1)) {
      return true;
    }
  }
  return false;
}

bool Walk::culledTriangle() {
  MeshCpu &c = c_;
  const std::array<std::uint32_t, 3> corners{c.t6, c.t4, c.t5};
  if (rowsOutside(c, corners)) {
    return true;
  }
  c.at = gteRead(psx::gte::kMac0);
  c.a3 = c.t6 << 16;
  if (negative(c.at)) {
    return true;
  }
  c.at = c.lw(kColumnMin);
  c.v1 = c.lw(kColumnMax);
  if (columnsOutside(c, corners)) {
    // The same culling branch has the depth average in its delay slot.
    if (negative(c.v0)) {
      c.gteOp(psx::gte::kAvsz3);
    }
    return true;
  }
  c.gteOp(psx::gte::kAvsz3);
  c.a3 = c.lw(kNearLimit);
  c.at = gteRead(psx::gte::kOtz);
  return false;
}

// Closer than the near limit, or deeper than the table: the OTZ test both handlers end their culling with.
// Quads compare the table depth signed, triangles unsigned.
bool Walk::outOfDepth(bool quad) {
  MeshCpu &c = c_;
  c.v0 = c.lw(kFree);
  if (quad) {
    c.sw(kVertices + 4u, c.t1);
  }
  c.a3 = MeshCpu::slt(c.at, c.a3);
  if (c.sgn(c.a3) > 0) {
    return true;
  }
  c.at <<= 2;
  c.a3 = quad ? MeshCpu::slt(c.at, c.s5) : (c.at < c.s5 ? 1u : 0u);
  if (quad) {
    c.sw(kVertices, c.t0);
  }
  return c.a3 == 0u;
}

Walk::Outcome Walk::primitive(const Handler &handler, std::uint32_t stride) {
  MeshCpu &c = c_;
  const bool quad = handler.shape.quad;
  if (quad) {
    projectQuad(handler, stride);
  } else {
    projectTriangle(stride);
  }
  const bool culled = quad ? culledQuad(handler) : culledTriangle();
  if (culled || outOfDepth(quad)) {
    return cull(handler);
  }
  return place(handler);
}

// A culled primitive that held a slot hands it back.
Walk::Outcome Walk::cull(const Handler &handler) {
  MeshCpu &c = c_;
  if (c.sgn(c.t9) <= 0) {
    return Outcome::Culled;
  }
  c.a3 = (c.t9 << 2) + c.s3;
  c.v1 = c.lw(kReleased);
  c.t3 = c.lb(c.a3 + 39u);
  c.sh(c.t8, 0);
  c.sh(c.v1, c.t9);
  c.v1 += 2u;
  c.sw(kReleased, c.v1);
  if (c.t3 != 0u) {
    release(handler, handler.sites.culled);
  }
  return Outcome::Culled;
}

void Walk::release(const Handler &handler, std::uint32_t site) {
  c_.ra = site;
  releaseSubtree(c_, handler.shape.textured ? PacketKind::Textured : PacketKind::Plain);
}

// What the guest's fill leaves in t3, which a triangle's subdivider reads: the page word of a textured packet,
// else the last corner's colour (a triangle's without the blue term).
std::uint32_t Walk::fillLeftover(PacketShape shape, const std::array<std::uint32_t, 4> &corners) const {
  if (shape.textured) {
    return m_.mem_r32(kPageWord);
  }
  if (shape.quad) {
    return packColour(m_.mem_r16s(corners[3] + 6u), false);
  }
  const auto bits = static_cast<std::uint32_t>(m_.mem_r16s(corners[2] + 6u));
  return ((bits & 0x7C00u) >> 7) + ((bits & 0x3E0u) << 6);
}

// The packet's UVs for the distance the primitive is drawn at: full, halved or quartered.
void Walk::rewriteUv(const Handler &handler, std::uint32_t level) {
  MeshCpu &c = c_;
  const std::uint32_t mask = level == 1u ? 0xFEFEu : 0xFCFCu;
  const auto scaled = [&](std::uint32_t value) {
    return level == 0u ? value : (value & mask) >> level;
  };
  if (handler.shape.quad) {
    c.t0 = c.lw(c.s0 + 4u);
    c.t2 = c.lw(c.s0 + 8u);
    c.t1 = c.t0 >> 16;
    c.t3 = c.t2 >> 16;
    c.t0 = scaled(c.t0);
    c.t1 = scaled(c.t1);
    c.t2 = scaled(c.t2);
    c.t3 = scaled(c.t3);
    c.sh(c.a3 + 12u, c.t0);
    c.sh(c.a3 + 24u, c.t1);
    c.sh(c.a3 + 36u, c.t3);
    c.sh(c.a3 + 48u, c.t2);
    return;
  }
  c.t1 = c.lw(c.s0 + 8u);
  c.t0 = c.lhu(c.s0 + 6u);
  c.t2 = c.t1 >> 16;
  c.t0 = scaled(c.t0);
  c.t1 = scaled(c.t1);
  c.t2 = scaled(c.t2);
  c.sh(c.a3 + 12u, c.t0);
  c.sh(c.a3 + 24u, c.t1);
  c.sh(c.a3 + 36u, c.t2);
}

// A textured packet's UV level follows the bucket offset it is linked at (t9). A quad rewrites its UVs when the
// level changes; a triangle always writes level 0, since its guard on v0 is a packet address and so always set.
void Walk::resample(const Handler &handler) {
  MeshCpu &c = c_;
  c.t2 = c.lh(kLodEnabled);
  c.at = c.t9 + c.a2;
  c.v1 = c.lbu(c.a3 + 38u);
  if (c.t2 == 0u) {
    return;
  }
  std::uint32_t level = 0;
  if (handler.shape.quad) {
    level = c.sgn(c.t9 - kSubdivideNear) <= 0 ? 0u : (c.sgn(c.t9 - kSubdivideFar) <= 0 ? 1u : 2u);
  } else if (!negative(c.v0)) {
    c.unported = true;
    return;
  }
  const bool unchanged = handler.shape.quad && c.v1 == level;
  c.v1 = level;
  if (unchanged) {
    return;
  }
  rewriteUv(handler, level);
  c.sb(c.a3 + 38u, c.v1);
  c.sb(kLodChanged, 1);
}

// A primitive near the camera is handed to its subdivider; a whole one drops the children a past subdivision
// left, then links.
Walk::Outcome Walk::subdivideOrLink(const Handler &handler) {
  MeshCpu &c = c_;
  if (!negative(c.v0)) {
    c.unported = true; // packet addresses are in KSEG0, so v0 is always negative here
    return Outcome::Handled;
  }
  c.t9 = kVertices;
  c.ra = handler.sites.subdivide;
  handler.subdivide(c);
  if (c.unported) {
    return Outcome::Handled;
  }
  c.t3 = c.lb(c.a3 + 39u);
  if (c.v0 != 0u) {
    return Outcome::Handled;
  }
  if (c.t3 != 0u) {
    c.t2 = c.a3;
    c.t0 = c.ra;
    c.t1 = c.at;
    release(handler, handler.sites.whole);
    c.ra = c.t0;
    c.at = c.t1;
    c.a3 = c.t2;
  }
  const PacketShape shape = handler.shape;
  const std::array<std::uint32_t, 4> xy{c.t4, c.t5, shape.quad ? c.t7 : c.t6, c.t6};
  linkPacket(m_, shape, c.a3, c.at, xy);
  return Outcome::Linked;
}

Walk::Outcome Walk::place(const Handler &handler) {
  MeshCpu &c = c_;
  c.v0 += 2u;
  if (c.sgn(c.t9) <= 0) {
    c.a3 = c.lhu(c.v0 - 2u);
    c.t9 = c.lw(kAltPackets);
    c.v1 = c.a3 << 2;
    if (c.a3 == 0u) {
      return Outcome::Dropped;
    }
    c.sw(kFree, c.v0);
    c.sh(c.t8, c.a3);
    c.v0 = c.v1 + c.t9;
    c.v1 += c.s3;
    const std::array<std::uint32_t, 4> corners{c.t0, c.t1, c.t2, c.t3};
    fillNewSlot(m_, handler.shape, c.s0, corners, c.a3, c.s3);
    c.t3 = fillLeftover(handler.shape, corners);
  }
  c.sb(kLodChanged, 0);
  c.sw(kDrawn, 0);
  c.at += c.s1;
  c.a3 = c.lhu(c.t8);
  c.t9 = c.lhu(c.at);
  c.a3 = (c.a3 << 2) + c.s3;
  if (handler.shape.textured) {
    resample(handler);
  } else {
    c.at = c.t9 + c.a2;
  }
  if (c.unported) {
    return Outcome::Handled;
  }
  return subdivideOrLink(handler);
}

} // namespace

bool StaticMeshDrawer::handles(std::uint32_t opcode) {
  return handlerOf(opcode) != nullptr;
}

std::optional<MeshDrawResult> StaticMeshDrawer::run(const EmitMemory &memory, const MeshDrawCall &call) {
  return Walk(memory, call).run();
}

std::uint32_t StaticMeshDrawer::orderingTable(const EmitMemory &memory) {
  return memory.mem_r32(kOrderingTable);
}

} // namespace ts2
