#include "render/rigid_mesh_drawer.h"

#include "core.h"
#include "gte_registers.h"
#include "render/mesh_culling.h"
#include "render/mesh_format.h"
#include "render/mesh_scratch.h"
#include "render/slot_packets.h"

#include <array>
#include <optional>
#include <span>

namespace ts2 {

using psx::present::EmitMemory;

namespace {

using namespace mesh_scratch;

constexpr std::uint32_t kScratchBytes = 0x100u;
constexpr std::uint32_t kBlendBias = kMeshScratch + 0x3Cu;
constexpr std::uint32_t kBucketOffsets = 0x800A1108u;
constexpr std::uint32_t kOtzLimit = 0x800A1184u;
constexpr std::uint32_t kPacketBytes = 0x40u;
constexpr std::uint32_t kMaxPrimitives = 0x10000u;

struct Kind {
  bool quad;
  bool textured;
  bool twoSided; // a quad is kept when either of its triangles faces the camera
};

PacketShape shapeOf(const Kind &kind) {
  return {kind.quad, kind.textured};
}

// Opcodes below 16 take 12-byte descriptors, from 16 on 4-byte ones. 2, 3, 8..15 and 18, 19, 21..23 are not
// reached by any replay and stay on the guest.
std::optional<Kind> kindOf(std::uint32_t opcode) {
  switch (opcode) {
  case 0:
    return Kind{true, true, false};
  case 1:
    return Kind{false, true, false};
  case 4:
    return Kind{true, true, true};
  case 16:
    return Kind{true, false, false};
  case 17:
    return Kind{false, false, false};
  case 20:
    return Kind{true, false, true};
  default:
    return std::nullopt;
  }
}

// Every projected depth in front of the near limit. The guest loads the next depth in the delay slot of each
// branch, so v1 holds a later depth or comparison than the test that ended the path.
bool nearOutside(CullRegisters &r, bool quad, std::uint32_t limit) {
  if (quad) {
    if (!below(gteRead(psx::gte::kSz1), limit)) {
      r.v1 = gteRead(kSz2);
      return false;
    }
    const bool second = below(gteRead(kSz2), limit);
    r.v1 = gteRead(kSz0);
    if (!second) {
      return false;
    }
    const bool third = below(gteRead(kSz3), limit);
    r.v1 = below(r.v1, limit) ? 1u : 0u;
    return third && r.v1 != 0u;
  }
  if (!below(gteRead(psx::gte::kSz1), limit)) {
    r.v1 = gteRead(kSz3);
    return false;
  }
  const bool second = below(gteRead(kSz2), limit);
  r.v1 = below(gteRead(kSz3), limit) ? 1u : 0u;
  return second && r.v1 != 0u;
}

enum class Outcome { Culled, Dropped, Linked };

class Walk {
public:
  Walk(const EmitMemory &memory, const MeshDrawCall &call) : m_(memory), call_(call) {}

  MeshDrawResult run();

private:
  void setupTexture(std::int32_t word) const;
  void storeCorners(std::uint32_t indices, bool quad) const;
  void command(const Kind &kind, std::uint32_t &s0, std::int32_t count);
  Outcome primitive(const Kind &kind, std::uint32_t s0, std::int32_t &count);
  void newSlot(const Kind &kind, std::uint32_t s0, const std::array<std::uint32_t, 4> &corners, std::uint32_t slot);
  void link(const Kind &kind, std::uint32_t bucket, const std::array<std::uint32_t, 4> &xy);
  void release(const Kind &kind);

  const EmitMemory &m_;
  const MeshDrawCall &call_;
  std::uint32_t vertices_ = 0;
  std::uint32_t bucketOffsets_ = 0;
  std::uint32_t otzLimit_ = 0;
  std::uint32_t slotTable_ = 0; // t8: this primitive's entry
  std::uint32_t packets_ = 0;
  std::uint32_t nothingDrawn_ = 1;
  std::int32_t slot_ = 0; // t9: the entry's slot, 0 for none
  CullRegisters r_;
};

void Walk::setupTexture(std::int32_t word) const {
  const std::uint32_t select = setTexturePage(m_, word);
  m_.mem_w32(kBlendBias, select == 0x40u ? 0x3C0u : 0u);
}

// The vertex addresses of a descriptor's index bytes, kept where the next primitive reads them.
void Walk::storeCorners(std::uint32_t indices, bool quad) const {
  m_.mem_w32(kCorners, vertices_ + ((indices << 3) & 0x7F8u));
  m_.mem_w32(kCorners + 4u, vertices_ + ((indices >> 5) & 0x7F8u));
  m_.mem_w32(kCorners + 8u, vertices_ + ((indices >> 13) & 0x7F8u));
  if (quad) {
    m_.mem_w32(kCorners + 12u, vertices_ + ((indices >> 21) & 0x7F8u));
  }
}

MeshDrawResult Walk::run() {
  const std::uint32_t mesh = call_.args[0];
  vertices_ = mesh + 4u;
  r_.v1 = call_.v1;
  m_.mem_w32(kNearLimit, m_.mem_r32(call_.args[3] + 0xCu));
  bucketOffsets_ = m_.mem_r32(kBucketOffsets);
  otzLimit_ = m_.mem_r32(kOtzLimit);
  slotTable_ = m_.mem_r32(kInstanceSlotTable);
  packets_ = m_.mem_r32(kSlotPacketBase);
  std::uint32_t s0 = vertices_ + (m_.mem_r32(mesh) << 3);
  while (true) {
    const std::int32_t word = m_.mem_r16s(s0);
    const std::uint32_t opcode = static_cast<std::uint32_t>(word) & 0x1Fu;
    if (word >= 0) {
      setupTexture(word);
    }
    const std::int32_t count = m_.mem_r16s(s0 + 2u);
    s0 += 4u;
    if (opcode >= 24u) {
      break;
    }
    command(*kindOf(opcode), s0, count);
  }
  return {nothingDrawn_, r_.v1};
}

void Walk::command(const Kind &kind, std::uint32_t &s0, std::int32_t count) {
  const std::uint32_t stride = kind.textured ? 12u : 4u;
  storeCorners(m_.mem_r32(s0), kind.quad);
  do {
    if (primitive(kind, s0, count) == Outcome::Culled) {
      release(kind);
    }
    slotTable_ += 2u;
    s0 += stride;
  } while (count != 0);
}

// A culled primitive that held a slot hands it back. The quad drawers load the cursor only when it did.
void Walk::release(const Kind &kind) {
  const bool held = slot_ > 0;
  if (held || !(kind.quad && !kind.twoSided)) {
    r_.v1 = m_.mem_r32(kReleased);
  }
  if (!held) {
    return;
  }
  m_.mem_w16(slotTable_, 0);
  m_.mem_w16(r_.v1, static_cast<std::uint16_t>(slot_));
  r_.v1 += 2u;
  m_.mem_w32(kReleased, r_.v1);
}

Outcome Walk::primitive(const Kind &kind, std::uint32_t s0, std::int32_t &count) {
  const std::array<std::uint32_t, 4> corner{m_.mem_r32(kCorners),
                                            m_.mem_r32(kCorners + 4u),
                                            m_.mem_r32(kCorners + 8u),
                                            kind.quad ? m_.mem_r32(kCorners + 12u) : 0u};
  const auto vertexZ = [&](std::uint32_t at) {
    return static_cast<std::uint32_t>(m_.mem_r16s(at + 4u));
  };
  std::uint32_t second = 0;
  if (kind.quad) {
    gteWrite(psx::gte::kVxy0, m_.mem_r32(corner[1]));
    gteWrite(psx::gte::kVz0, m_.mem_r32(corner[1] + 4u));
    gte_op(&m_.core(), psx::gte::kRtps);
    gteWrite(psx::gte::kVxy0, m_.mem_r32(corner[3]));
    gteWrite(psx::gte::kVz0, vertexZ(corner[3]));
    gteWrite(psx::gte::kVxy1, m_.mem_r32(corner[2]));
    gteWrite(psx::gte::kVz1, vertexZ(corner[2]));
    gteWrite(psx::gte::kVxy2, m_.mem_r32(corner[0]));
    gteWrite(psx::gte::kVz2, m_.mem_r32(corner[0] + 4u));
    second = gteRead(psx::gte::kSxy2);
  } else {
    gteWrite(psx::gte::kVxy2, m_.mem_r32(corner[0]));
    gteWrite(psx::gte::kVz2, m_.mem_r32(corner[0] + 4u));
    gteWrite(psx::gte::kVxy1, m_.mem_r32(corner[1]));
    gteWrite(psx::gte::kVz1, m_.mem_r32(corner[1] + 4u));
    gteWrite(psx::gte::kVxy0, m_.mem_r32(corner[2]));
    gteWrite(psx::gte::kVz0, m_.mem_r32(corner[2] + 4u));
  }
  --count;
  gte_op(&m_.core(), psx::gte::kRtpt);
  slot_ = m_.mem_r16s(slotTable_);
  const std::uint32_t next = m_.mem_r32(s0 + (kind.textured ? 12u : 4u));
  r_.v1 = m_.mem_r32(kRowMax);
  r_.at = m_.mem_r32(kRowMin);
  storeCorners(next, kind.quad);

  const bool oneSidedQuad = kind.quad && !kind.twoSided;
  if (oneSidedQuad) {
    gte_op(&m_.core(), psx::gte::kNclip);
    r_.a3 = gteRead(psx::gte::kMac0);
    if (negative(r_.a3)) {
      return Outcome::Culled;
    }
  }
  // Packet corners 0..3 in the order the projections leave them.
  std::array<std::uint32_t, 4> xy{};
  std::array<std::uint32_t, 4> tests{};
  if (kind.quad) {
    xy = {gteRead(psx::gte::kSxy2), second, gteRead(psx::gte::kSxy0), gteRead(psx::gte::kSxy1)};
    tests = {xy[3], xy[0], second, xy[2]};
  } else {
    xy = {gteRead(psx::gte::kSxy2), gteRead(psx::gte::kSxy1), gteRead(psx::gte::kSxy0), 0u};
    tests = {xy[2], xy[0], xy[1], 0u};
  }
  const std::span<const std::uint32_t> corners = std::span(tests).first(kind.quad ? 4u : 3u);
  if (!oneSidedQuad) {
    gte_op(&m_.core(), psx::gte::kNclip);
  }
  if (rowsOutside(r_, corners)) {
    return Outcome::Culled;
  }
  if (kind.quad && kind.twoSided) {
    r_.at = gteRead(psx::gte::kMac0);
    gteWrite(psx::gte::kSxy2, second);
    if (negative(r_.at)) {
      gte_op(&m_.core(), psx::gte::kNclip);
      r_.v1 = gteRead(psx::gte::kMac0);
      if (negative(r_.v1)) {
        return Outcome::Culled;
      }
    }
  } else if (!kind.quad) {
    r_.at = gteRead(psx::gte::kMac0);
    if (negative(r_.at)) {
      return Outcome::Culled;
    }
  }
  r_.at = m_.mem_r32(kColumnMin);
  r_.v1 = m_.mem_r32(kColumnMax);
  const std::uint32_t average = kind.quad ? psx::gte::kAvsz4 : psx::gte::kAvsz3;
  if (columnsOutside(r_, corners)) {
    // The branch that culls a primitive left of the screen has the depth average in its delay slot.
    if (negative(r_.v0)) {
      gte_op(&m_.core(), average);
    }
    return Outcome::Culled;
  }
  gte_op(&m_.core(), average);
  const std::uint32_t limit = m_.mem_r32(kNearLimit) << 2;
  r_.v0 = m_.mem_r32(kFree);
  if (nearOutside(r_, kind.quad, limit)) {
    return Outcome::Culled;
  }
  r_.at = gteRead(psx::gte::kOtz);
  r_.a3 = r_.at - otzLimit_;
  if (static_cast<std::int32_t>(r_.a3) > 0) {
    return Outcome::Culled;
  }
  const std::uint32_t bucket = (static_cast<std::uint32_t>(static_cast<std::int32_t>(r_.at) >> 1)) & 0xFFFEu;
  r_.v0 += 2u;
  if (slot_ <= 0) {
    r_.a3 = m_.mem_r16(r_.v0 - 2u);
    r_.v1 = r_.a3 << 2;
    if (r_.a3 == 0u) {
      return Outcome::Dropped;
    }
    m_.mem_w32(kFree, r_.v0);
    m_.mem_w16(slotTable_, static_cast<std::uint16_t>(r_.a3));
    newSlot(kind, s0, corner, r_.a3);
  }
  nothingDrawn_ = 0;
  link(kind, bucket, xy);
  return Outcome::Linked;
}

void Walk::newSlot(const Kind &kind,
                   std::uint32_t s0,
                   const std::array<std::uint32_t, 4> &corners,
                   std::uint32_t slot) {
  const SlotPair pair = fillNewSlot(m_, shapeOf(kind), s0, corners, slot, packets_);
  r_.v0 = pair.alt;
  r_.v1 = pair.own;
}

void Walk::link(const Kind &kind, std::uint32_t bucket, const std::array<std::uint32_t, 4> &xy) {
  const std::uint32_t packet = packets_ + (static_cast<std::uint32_t>(m_.mem_r16(slotTable_)) << 2);
  const std::uint32_t head = m_.mem_r16(bucketOffsets_ + bucket) + call_.args[2];
  linkPacket(m_, shapeOf(kind), packet, head, xy);
}

} // namespace

bool RigidMeshDrawer::ported(const EmitMemory &memory, const MeshDrawCall &call) {
  const std::optional<ResidentMeshStream> stream = walkResidentMesh(memory, call.args[0]);
  if (!stream || stream->layout.hasAuxiliaryVertexRecords || stream->primitives >= kMaxPrimitives) {
    return false;
  }
  for (const ResidentMeshCommand &command : stream->commands) {
    if (!command.terminal && !kindOf(command.opcode)) {
      return false;
    }
  }
  return true;
}

MeshDrawResult RigidMeshDrawer::run(const EmitMemory &memory, const MeshDrawCall &call) {
  return Walk(memory, call).run();
}

MeshDrawInputs RigidMeshDrawer::inputs(const EmitMemory &m, const MeshDrawCall &call) {
  const std::optional<ResidentMeshStream> stream = walkResidentMesh(m, call.args[0]);
  MeshDrawInputs inputs;
  inputs.primitives = stream->primitives;
  inputs.meshBytes = stream->end - call.args[0];
  std::vector<MemoryRange> &ranges = inputs.ranges;
  ranges.push_back({call.args[0], stream->end - call.args[0]});
  ranges.push_back({call.args[3] + 0xCu, sizeof(std::uint32_t)});
  for (const std::uint32_t global : {kBucketOffsets, kOtzLimit, kInstanceSlotTable}) {
    ranges.push_back({global, sizeof(std::uint32_t)});
  }
  ranges.push_back({kTexturePages, kTexturePageBytes});
  const std::uint32_t table = m.mem_r32(kInstanceSlotTable);
  ranges.push_back({table, stream->primitives * 2u});
  const std::uint32_t packets = m.mem_r32(kSlotPacketBase);
  const std::uint32_t alt = m.mem_r32(kAltPackets);
  std::uint32_t unslotted = 0;
  std::uint32_t slotted = 0;
  for (std::uint32_t entry = 0; entry != stream->primitives; ++entry) {
    const std::uint32_t slot = m.mem_r16(table + entry * 2u);
    if (slot == 0u) {
      ++unslotted;
    } else {
      ++slotted;
      ranges.push_back({packets + slot * 4u, kPacketBytes});
    }
  }
  const std::uint32_t free = m.mem_r32(kFree);
  ranges.push_back({free, (unslotted + 1u) * 2u});
  for (std::uint32_t i = 0; i != unslotted; ++i) {
    const std::uint32_t slot = m.mem_r16(free + i * 2u);
    if (slot == 0u) {
      break;
    }
    ranges.push_back({packets + slot * 4u, kPacketBytes});
    ranges.push_back({alt + slot * 4u, kPacketBytes});
  }
  ranges.push_back({m.mem_r32(kReleased), (slotted + 1u) * 2u});
  ranges.push_back({kMeshScratch, kScratchBytes});
  inputs.ranges = unionOf(std::move(ranges));
  return inputs;
}

} // namespace ts2
