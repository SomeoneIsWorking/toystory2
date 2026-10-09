#include "render/part_face_drawers.h"

#include "core.h"
#include "emit_memory.h"
#include "gte_registers.h"
#include "native_dispatch.h"
#include "render/actor_producers.h"
#include "render/part_draw_state.h"
#include "runtime/toystory2_context.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <lucent/log.h>
#include <optional>
#include <span>

namespace ts2 {

using psx::present::EmitMemory;

namespace {

constexpr std::uint32_t kScratch = 0x1F800000u;         // s0..s7 are spilled here
constexpr std::uint32_t kNormalBase = kScratch + 0x20u; // 0x8001DFE4: a3, the colour normals are lit on
constexpr std::uint32_t kSavedBackground = kScratch + 0x24u;
constexpr std::uint32_t kSavedFacing = kScratch + 0xE0u;
constexpr std::uint32_t kOrderingTable = PartFaceDrawers::kOrderingTable; // base the bucket offsets are added to
constexpr std::uint32_t kVisibleRows = 0x800A1100u;                       // >= 0: rows [0, v); < 0: rows [v, 240)
constexpr std::uint32_t kPacketCursor = PartFaceDrawers::kPacketCursor;
constexpr std::uint32_t kFacing = 0x800A13B4u;        // sign flips which winding faces the camera
constexpr std::uint32_t kBucketOffsets = 0x800A12F8u; // u16 per OTZ (index OTZ * 4)
constexpr std::uint32_t kDepthLimit = 0x800A135Cu;    // OTZ limit; the drawers scale it by 4
constexpr std::uint32_t kTexturePages = 0x800CD200u;  // 16 bytes per page: tpage, clut base
constexpr std::uint32_t kPartNormals = 0x0Cu;         // u16 count, normals of 8 bytes, u16 index per corner
constexpr std::uint32_t kPartFaces = 0x20u;
constexpr std::uint32_t kTexturePageBytes = 16u;
constexpr std::uint32_t kLightVectorBytes = 12u;
constexpr std::uint32_t kPacketSlack = 0x40u; // a culled face still writes its early words at the cursor

// GTE light colour matrix and background colour, as control registers.
constexpr std::uint32_t kLightColour = 16u;
constexpr std::uint32_t kBackground = 13u;

constexpr std::uint32_t kKindMask = 0x3C000000u;
constexpr std::uint32_t kQuadBytes = 0x30u;
constexpr std::uint32_t kTriangleBytes = 0x24u;
constexpr std::uint32_t kQuadCorners = 4u;
constexpr std::uint32_t kTriangleCorners = 3u;
constexpr std::uint32_t kGt4Bytes = 0x34u;
constexpr std::uint32_t kGt3Bytes = 0x28u;
constexpr std::uint32_t kGt4Length = 0x0C000000u;
constexpr std::uint32_t kGt3Length = 0x09000000u;
constexpr std::int32_t kNearSlack = 0x5DC;
constexpr std::int32_t kNearestBucket = 0x14;

enum class Rows { Inside, Above, Below };

struct Walk {
  const EmitMemory &mem;
  std::uint32_t part;
  std::uint32_t face;
  std::uint32_t ot;
  std::uint32_t packet;
  std::int32_t top;
  std::int32_t bottom;
  std::uint32_t facing;
  std::uint32_t buckets;
  std::uint32_t depthLimit;
  std::uint32_t tpage;
  std::uint32_t clutBase;
  std::uint32_t normals; // 0x8001DFE4 only: the normal table and the face's corner indices
  std::uint32_t corners;
};

// What the prelit and normal-lit drawers know of a face once it passed the depth and facing tests.
struct Projected {
  std::array<std::uint32_t, 4> xy;
  std::array<std::uint32_t, 4> colours; // packet order: colour 0..3
  std::uint32_t uv0;
  Rows rows;
};

std::uint32_t gteRead(std::uint32_t reg) {
  return gte_read_data(reg);
}

void gteWrite(std::uint32_t reg, std::uint32_t value) {
  gte_write_data(reg, value);
}

// The prologue every drawer shares; the face list's first word picks the texture page for every face.
Walk begin(const EmitMemory &m, std::uint32_t part) {
  const std::uint32_t faces = m.mem_r32(part + kPartFaces);
  const auto rows = static_cast<std::int32_t>(m.mem_r32(kVisibleRows));
  Walk walk{.mem = m,
            .part = part,
            .face = faces,
            .ot = m.mem_r32(kOrderingTable),
            .packet = m.mem_r32(kPacketCursor),
            .top = rows < 0 ? static_cast<std::int32_t>(static_cast<std::uint32_t>(rows) << 16) : 0,
            .bottom = rows < 0 ? 0x00F00000 : static_cast<std::int32_t>(static_cast<std::uint32_t>(rows) << 16),
            .facing = m.mem_r32(kFacing),
            .buckets = m.mem_r32(kBucketOffsets),
            .depthLimit = m.mem_r32(kDepthLimit) << 2,
            .tpage = 0,
            .clutBase = 0,
            .normals = 0,
            .corners = 0};
  const bool quad = (m.mem_r32(faces) & kKindMask) == kKindMask;
  const auto select = static_cast<std::uint32_t>(m.mem_r8s(faces + (quad ? 0x2Fu : 0x23u)));
  const std::uint32_t page = kTexturePages + ((select & 0x1Fu) << 4);
  walk.clutBase = m.mem_r16(page + 2u);
  walk.tpage = m.mem_r16(page) + (select & 0x60u);
  return walk;
}

void end(Walk &walk) {
  walk.mem.mem_w32(kPacketCursor, walk.packet);
}

// SZ1 in front of the near plane and within the depth limit.
bool nearEnough(const Walk &walk) {
  const auto sz1 = static_cast<std::int32_t>(gteRead(psx::gte::kSz1));
  return sz1 >= 0 && sz1 - kNearSlack < static_cast<std::int32_t>(walk.depthLimit);
}

// Each packed SXY word is compared whole against the row bounds; Below is the last vertex at or under the
// bottom, where the guest has already run the delay slot of its final branch.
Rows rows(const Walk &walk, std::span<const std::uint32_t> xy) {
  for (const std::uint32_t word : xy) {
    const auto v = static_cast<std::int32_t>(word);
    if (v >= walk.top && v < walk.bottom) {
      return Rows::Inside;
    }
  }
  return static_cast<std::int32_t>(xy.back()) < walk.top ? Rows::Above : Rows::Below;
}

// The OT bucket for the current OTZ, or 0 when the depth falls outside the table.
std::uint32_t bucket(const Walk &walk, std::uint32_t otz) {
  if (static_cast<std::int32_t>(otz) < kNearestBucket || (otz << 2) >= walk.depthLimit) {
    return 0u;
  }
  return walk.mem.mem_r16(walk.buckets + (otz << 2)) + walk.ot;
}

void insert(Walk &walk, std::uint32_t at, std::uint32_t length, std::uint32_t bytes, std::uint32_t uv0) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t next = m.mem_r32(at);
  m.mem_w32(at, walk.packet & 0x00FFFFFFu);
  m.mem_w32(walk.packet, next + length);
  m.mem_w16(walk.packet + 0x1Au, static_cast<std::uint16_t>(walk.tpage));
  m.mem_w16(walk.packet + 0x0Eu, static_cast<std::uint16_t>(((uv0 >> 12) & 0xCu) + ((uv0 >> 6) & 3u) + walk.clutBase));
  walk.packet += bytes;
}

void storeColours(const EmitMemory &m, std::uint32_t packet, std::span<const std::uint32_t> colours) {
  for (std::size_t i = 1; i != colours.size(); ++i) {
    m.mem_w32(packet + 4u + static_cast<std::uint32_t>(i) * 0x0Cu, colours[i]);
  }
}

// Quad record: +0 colour 0 and type, +4/+C/+1C/+14 xy of vertices 0..3, then each vertex's z/uv word,
// +24 colour 1, +28 colour 3, +2C colour 2 (negative: two-sided). Only the prelit drawer stores uv0 early.
std::optional<Projected> projectQuad(Walk &walk, std::uint32_t head, bool storeUv0) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t f = walk.face;
  const std::uint32_t p = walk.packet;
  gteWrite(psx::gte::kVxy1, m.mem_r32(f + 0x0Cu));
  gteWrite(psx::gte::kVxy2, m.mem_r32(f + 0x1Cu));
  const std::uint32_t zuv0 = m.mem_r32(f + 0x08u);
  const std::uint32_t zuv1 = m.mem_r32(f + 0x10u);
  const std::uint32_t zuv2 = m.mem_r32(f + 0x20u);
  gteWrite(psx::gte::kVz0, zuv0);
  gteWrite(psx::gte::kVz1, zuv1);
  gteWrite(psx::gte::kVz2, zuv2);
  gte_op(&m.core(), psx::gte::kRtpt);
  const std::uint32_t xy3 = m.mem_r32(f + 0x14u);
  const std::uint32_t zuv3 = m.mem_r32(f + 0x18u);
  if (storeUv0) {
    m.mem_w16(p + 0x0Cu, static_cast<std::uint16_t>(zuv0 >> 16));
  }
  m.mem_w16(p + 0x18u, static_cast<std::uint16_t>(zuv1 >> 16));
  m.mem_w16(p + 0x24u, static_cast<std::uint16_t>(zuv2 >> 16));
  m.mem_w16(p + 0x30u, static_cast<std::uint16_t>(zuv3 >> 16));
  if (!nearEnough(walk)) {
    return std::nullopt;
  }
  const std::uint32_t xy0 = gteRead(psx::gte::kSxy0);
  const std::uint32_t xy1 = gteRead(psx::gte::kSxy1);
  const std::uint32_t xy2 = gteRead(psx::gte::kSxy2);
  gte_op(&m.core(), psx::gte::kNclip);
  gteWrite(psx::gte::kVxy0, xy3);
  gteWrite(psx::gte::kVz0, zuv3);
  const std::uint32_t front = gteRead(psx::gte::kMac0) ^ walk.facing;
  gte_op(&m.core(), psx::gte::kRtps);
  const std::uint32_t colour2 = m.mem_r32(f + 0x2Cu);
  const std::uint32_t colour1 = m.mem_r32(f + 0x24u);
  const std::uint32_t colour3 = m.mem_r32(f + 0x28u);
  if (static_cast<std::int32_t>(colour2) >= 0) {
    gte_op(&m.core(), psx::gte::kNclip);
    if (static_cast<std::int32_t>(front) < 0 && static_cast<std::int32_t>(gteRead(psx::gte::kMac0) ^ walk.facing) > 0) {
      return std::nullopt;
    }
  }
  const std::uint32_t screen3 = gteRead(psx::gte::kSxy2);
  gte_op(&m.core(), psx::gte::kAvsz4);
  Projected face{.xy = {xy0, xy1, xy2, screen3},
                 .colours = {head, colour1, colour2, colour3},
                 .uv0 = zuv0 >> 16,
                 .rows = Rows::Inside};
  face.rows = rows(walk, face.xy);
  return face;
}

// Triangle record: +0 colour 0 and type, +4/+C/+14 xy, the next word of each z and uv, +1C colour 1,
// +20 colour 2 (negative: two-sided). Shared by 0x8001C920 and 0x8001DFE4.
std::optional<Projected> projectTriangle(Walk &walk, std::uint32_t head) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t f = walk.face;
  const std::uint32_t p = walk.packet;
  gteWrite(psx::gte::kVxy2, m.mem_r32(f + 0x14u));
  const std::uint32_t zuv0 = m.mem_r32(f + 0x08u);
  const std::uint32_t zuv1 = m.mem_r32(f + 0x10u);
  const std::uint32_t zuv2 = m.mem_r32(f + 0x18u);
  gteWrite(psx::gte::kVz0, zuv0);
  gteWrite(psx::gte::kVz1, zuv1);
  gteWrite(psx::gte::kVz2, zuv2);
  gte_op(&m.core(), psx::gte::kRtpt);
  const std::uint32_t colour1 = m.mem_r32(f + 0x1Cu);
  const std::uint32_t colour2 = m.mem_r32(f + 0x20u);
  m.mem_w16(p + 0x0Cu, static_cast<std::uint16_t>(zuv0 >> 16));
  m.mem_w16(p + 0x18u, static_cast<std::uint16_t>(zuv1 >> 16));
  m.mem_w16(p + 0x24u, static_cast<std::uint16_t>(zuv2 >> 16));
  if (!nearEnough(walk)) {
    return std::nullopt;
  }
  const std::uint32_t xy0 = gteRead(psx::gte::kSxy0);
  const std::uint32_t xy1 = gteRead(psx::gte::kSxy1);
  const std::uint32_t xy2 = gteRead(psx::gte::kSxy2);
  gte_op(&m.core(), psx::gte::kNclip);
  const std::uint32_t front = gteRead(psx::gte::kMac0) ^ walk.facing;
  gte_op(&m.core(), psx::gte::kAvsz3);
  if (static_cast<std::int32_t>(colour2) >= 0 && static_cast<std::int32_t>(front) < 0) {
    return std::nullopt;
  }
  Projected face{
      .xy = {xy0, xy1, xy2, 0u}, .colours = {head, colour1, colour2, 0u}, .uv0 = zuv0 >> 16, .rows = Rows::Inside};
  face.rows = rows(walk, std::span(face.xy).first(kTriangleCorners));
  return face;
}

void prelitQuad(Walk &walk, std::uint32_t head) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectQuad(walk, head, true);
  if (!face) {
    return;
  }
  if (face->rows != Rows::Above) {
    m.mem_w32(p + 0x08u, face->xy[0]);
  }
  if (face->rows != Rows::Inside) {
    return;
  }
  m.mem_w32(p + 0x2Cu, face->xy[3]);
  const std::uint32_t otz = gteRead(psx::gte::kOtz);
  m.mem_w32(p + 0x14u, face->xy[1]);
  m.mem_w32(p + 0x20u, face->xy[2]);
  const std::uint32_t at = bucket(walk, otz);
  if (at == 0u) {
    return;
  }
  m.mem_w32(p + 0x04u, head);
  storeColours(m, p, face->colours);
  insert(walk, at, kGt4Length, kGt4Bytes, face->uv0);
}

void prelitTriangle(Walk &walk, std::uint32_t head) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectTriangle(walk, head);
  if (!face) {
    return;
  }
  if (face->rows != Rows::Above) {
    m.mem_w32(p + 0x04u, head);
  }
  if (face->rows != Rows::Inside) {
    return;
  }
  storeColours(m, p, std::span(face->colours).first(kTriangleCorners));
  m.mem_w32(p + 0x08u, face->xy[0]);
  const std::uint32_t otz = gteRead(psx::gte::kOtz);
  m.mem_w32(p + 0x14u, face->xy[1]);
  m.mem_w32(p + 0x20u, face->xy[2]);
  const std::uint32_t at = bucket(walk, otz);
  if (at == 0u) {
    return;
  }
  insert(walk, at, kGt3Length, kGt3Bytes, face->uv0);
}

// The corner index slot each packet corner is lit from: a quad's packet corners 2 and 3 are its
// record's vertices 3 and 2.
constexpr std::array<std::uint32_t, 4> kQuadNormalSlots{0u, 1u, 3u, 2u};
constexpr std::array<std::uint32_t, 3> kTriangleNormalSlots{0u, 1u, 2u};

std::array<std::uint32_t, 3> normal(const Walk &walk, std::uint32_t slot) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t at = walk.normals + (static_cast<std::uint32_t>(m.mem_r16(walk.corners + slot * 2u)) << 3);
  return {static_cast<std::uint32_t>(m.mem_r16s(at)),
          static_cast<std::uint32_t>(m.mem_r16s(at + 2u)),
          static_cast<std::uint32_t>(m.mem_r16s(at + 4u))};
}

void setIr(const std::array<std::uint32_t, 3> &ir) {
  gteWrite(psx::gte::kIr1, ir[0]);
  gteWrite(psx::gte::kIr2, ir[1]);
  gteWrite(psx::gte::kIr3, ir[2]);
}

// 0x8001DFE4 lights each corner with CC then DCPL, storing each result one step behind; the last is stored
// only once the face has a bucket.
void lightCorners(Walk &walk,
                  std::uint32_t base,
                  std::span<const std::uint32_t> colours,
                  std::span<const std::uint32_t> slots) {
  const EmitMemory &m = walk.mem;
  setIr(normal(walk, slots[0]));
  gte_op(&m.core(), psx::gte::kCc);
  for (std::size_t i = 0; i != colours.size(); ++i) {
    setIr({(gteRead(psx::gte::kMac1) << 1) + 0x1000u,
           (gteRead(psx::gte::kMac2) << 1) + 0x1000u,
           (gteRead(psx::gte::kMac3) << 1) + 0x1000u});
    gteWrite(psx::gte::kRgbc, colours[i]);
    gteWrite(psx::gte::kIr0, 0u);
    gte_op(&m.core(), psx::gte::kDcpl);
    if (i + 1 == colours.size()) {
      return;
    }
    gteWrite(psx::gte::kRgbc, base);
    setIr(normal(walk, slots[i + 1]));
    m.mem_w32(walk.packet + 4u + static_cast<std::uint32_t>(i) * 0x0Cu, gteRead(psx::gte::kRgb2));
    gte_op(&m.core(), psx::gte::kCc);
  }
}

// A negative first corner index draws the face's own colours unlit.
void normalLitQuad(Walk &walk, std::uint32_t head) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectQuad(walk, head, false);
  if (!face || face->rows != Rows::Inside) {
    return;
  }
  const std::uint32_t base = m.mem_r32(kNormalBase);
  const bool unlit = m.mem_r16s(walk.corners) < 0;
  m.mem_w32(p + 0x08u, face->xy[0]);
  m.mem_w32(p + 0x14u, face->xy[1]);
  m.mem_w32(p + 0x20u, face->xy[2]);
  gteWrite(psx::gte::kRgbc, base);
  m.mem_w16(p + 0x0Cu, static_cast<std::uint16_t>(face->uv0));
  m.mem_w32(p + 0x2Cu, face->xy[3]);
  if (unlit) {
    m.mem_w32(p + 0x04u, head);
  } else {
    lightCorners(walk, base, face->colours, kQuadNormalSlots);
  }
  const std::uint32_t at = bucket(walk, gteRead(psx::gte::kOtz));
  if (at == 0u) {
    return;
  }
  if (unlit) {
    storeColours(m, p, face->colours);
  } else {
    m.mem_w32(p + 0x28u, gteRead(psx::gte::kRgb2));
  }
  insert(walk, at, kGt4Length, kGt4Bytes, face->uv0);
}

void normalLitTriangle(Walk &walk, std::uint32_t head) {
  const EmitMemory &m = walk.mem;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectTriangle(walk, head);
  if (!face) {
    return;
  }
  if (face->rows != Rows::Above) {
    m.mem_w32(p + 0x08u, face->xy[0]);
  }
  if (face->rows != Rows::Inside) {
    return;
  }
  m.mem_w32(p + 0x14u, face->xy[1]);
  m.mem_w32(p + 0x20u, face->xy[2]);
  const std::uint32_t base = m.mem_r32(kNormalBase);
  const bool unlit = m.mem_r16s(walk.corners) < 0;
  gteWrite(psx::gte::kRgbc, base);
  const std::span<const std::uint32_t> colours = std::span(face->colours).first(kTriangleCorners);
  if (unlit) {
    m.mem_w32(p + 0x04u, head);
  } else {
    lightCorners(walk, base, colours, kTriangleNormalSlots);
  }
  const std::uint32_t at = bucket(walk, gteRead(psx::gte::kOtz));
  if (at == 0u) {
    return;
  }
  if (unlit) {
    storeColours(m, p, colours);
  } else {
    m.mem_w32(p + 0x1Cu, gteRead(psx::gte::kRgb2));
  }
  insert(walk, at, kGt3Length, kGt3Bytes, face->uv0);
}

using FaceDraw = void (*)(Walk &, std::uint32_t);

// The GTE load in the delay slot of the branch that tells a triangle from the terminator.
struct TriangleLead {
  std::uint32_t reg;
  std::uint32_t offset;
};

// The face loop: VXY0 is loaded for every record and the triangle lead for every non-quad, the
// terminator included. Corner indices advance per face, drawn or not.
void walkFaces(Walk &walk, FaceDraw quad, FaceDraw triangle, TriangleLead lead) {
  const EmitMemory &m = walk.mem;
  for (std::uint32_t index = 0;; ++index) {
    const std::uint32_t head = m.mem_r32(walk.face);
    gteWrite(psx::gte::kVxy0, m.mem_r32(walk.face + 4u));
    const std::uint32_t kind = head & kKindMask;
    const bool isQuad = kind == kKindMask;
    if (!isQuad) {
      gteWrite(lead.reg, m.mem_r32(walk.face + lead.offset));
    }
    if (kind == 0u) {
      break;
    }
    const psx::present::ElementScope face(m, index);
    (isQuad ? quad : triangle)(walk, head);
    walk.face += isQuad ? kQuadBytes : kTriangleBytes;
    walk.corners += (isQuad ? kQuadCorners : kTriangleCorners) * 2u;
  }
  end(walk);
}

void drawPrelit(const EmitMemory &m, const PartDrawCall &call) {
  Walk walk = begin(m, call.args[0]);
  walkFaces(walk, &prelitQuad, &prelitTriangle, TriangleLead{psx::gte::kVxy1, 0x0Cu});
}

// a1 points at the light vector (three words, low halves used), a2 is the background colour for the
// walk, a3 the base colour.
void drawNormalLit(const EmitMemory &m, const PartDrawCall &call) {
  const std::uint32_t lx = m.mem_r16(call.args[1]);
  const std::uint32_t ly = m.mem_r16(call.args[1] + 4u);
  const std::uint32_t lz = m.mem_r16(call.args[1] + 8u);
  const std::uint32_t xy = (ly << 16) + lx;
  const std::uint32_t zx = (lx << 16) + lz;
  gte_write_ctrl(kLightColour, xy);
  gte_write_ctrl(kLightColour + 1u, zx);
  gte_write_ctrl(kLightColour + 2u, (lz << 16) + ly);
  gte_write_ctrl(kLightColour + 3u, xy);
  gte_write_ctrl(kLightColour + 4u, zx);
  m.mem_w32(kNormalBase, call.args[3]);
  for (std::uint32_t i = 0; i != 3u; ++i) {
    m.mem_w32(kSavedBackground + i * 4u, gte_read_ctrl(kBackground + i));
    gte_write_ctrl(kBackground + i, call.args[2]);
  }
  const std::uint32_t normals = m.mem_r32(call.args[0] + kPartNormals);
  Walk walk = begin(m, call.args[0]);
  m.mem_w32(kSavedFacing, walk.facing);
  walk.normals = normals + 4u;
  walk.corners = walk.normals + (static_cast<std::uint32_t>(m.mem_r16(normals)) << 3);
  walkFaces(walk, &normalLitQuad, &normalLitTriangle, TriangleLead{psx::gte::kVxy1, 0x0Cu});
  for (std::uint32_t i = 0; i != 3u; ++i) {
    gte_write_ctrl(kBackground + i, m.mem_r32(kSavedBackground + i * 4u));
  }
}

} // namespace

void PartFaceDrawers::run(const EmitMemory &memory, const PartDrawCall &call) {
  switch (call.drawer) {
  case kPrelit:
    drawPrelit(memory, call);
    return;
  case kNormalLit:
    drawNormalLit(memory, call);
    return;
  default:
    lucent::error("ts2-actors", "0x{:08X} is not a ported part drawer", call.drawer);
    std::abort();
  }
}

PartDrawInputs PartFaceDrawers::inputs(const EmitMemory &m, const PartDrawCall &call) {
  const std::uint32_t part = call.args[0];
  const std::uint32_t faces = m.mem_r32(part + kPartFaces);
  std::uint32_t end = faces;
  std::uint32_t corners = 0;
  std::uint32_t count = 0;
  for (std::uint32_t kind = m.mem_r32(end) & kKindMask; kind != 0u; kind = m.mem_r32(end) & kKindMask, ++count) {
    const bool quad = kind == kKindMask;
    end += quad ? kQuadBytes : kTriangleBytes;
    corners += quad ? kQuadCorners : kTriangleCorners;
  }
  // The terminator's head word ends the list; the loads the guest makes behind it feed nothing.
  PartDrawInputs inputs;
  inputs.ranges.push_back({part, ActorProducers::kPartBytes});
  inputs.ranges.push_back({faces, end - faces + static_cast<std::uint32_t>(sizeof(std::uint32_t))});
  const bool quad = (m.mem_r32(faces) & kKindMask) == kKindMask;
  const auto select = static_cast<std::uint32_t>(m.mem_r8s(faces + (quad ? 0x2Fu : 0x23u)));
  inputs.ranges.push_back({kTexturePages + ((select & 0x1Fu) << 4), kTexturePageBytes});
  if (call.drawer == kNormalLit) {
    const std::uint32_t normals = m.mem_r32(part + kPartNormals);
    inputs.ranges.push_back({normals, 4u + (static_cast<std::uint32_t>(m.mem_r16(normals)) << 3) + corners * 2u});
    inputs.ranges.push_back({call.args[1], kLightVectorBytes});
  }
  for (const std::uint32_t global :
       {kVisibleRows, kOrderingTable, kPacketCursor, kFacing, kBucketOffsets, kDepthLimit}) {
    inputs.ranges.push_back({global, sizeof(std::uint32_t)});
  }
  inputs.packetBytes = count * kGt4Bytes + kPacketSlack;
  return inputs;
}

namespace {

// s0..s7 are spilled to the scratchpad before the body, and v0 returns its base.
template <std::uint32_t Drawer> void overrideDrawer(Core *core) {
  for (std::uint32_t s = 0; s != 8u; ++s) {
    core->mem_w32(kScratch + s * 4u, core->r[16u + s]);
  }
  const PartDrawCall call{Drawer, {core->r[4], core->r[5], core->r[6], core->r[7]}};
  const EmitMemory guest(*core);
  if (const std::optional<std::uint32_t> generation = context(*core).actorProducers.drawing()) {
    const psx::present::EmissionScope::Guard object(
        core->emission, ActorProducers::kActorRenderer, ActorProducers::partObject(call.args[0], *generation), 0);
    const PartDrawRecorder recorder(*core, call);
    PartFaceDrawers::run(guest, call);
    recorder.save(*core);
  } else {
    PartFaceDrawers::run(guest, call);
  }
  core->r[2] = kScratch;
  if constexpr (Drawer == PartFaceDrawers::kNormalLit) {
    // 0x8001DFE4 leaves its part-table cursor (*(a0+0xC) + 4) in v1
    core->r[3] = guest.mem_r32(call.args[0] + 0xCu) + 4u;
  }
}

} // namespace

void PartFaceDrawers::install(Core &core) {
  psx::cpu::installNativeOverride(core, kPrelit, "prelit-part-drawer", &overrideDrawer<kPrelit>);
  psx::cpu::installNativeOverride(core, kNormalLit, "normal-lit-part-drawer", &overrideDrawer<kNormalLit>);
}

} // namespace ts2
