#include "render/part_face_drawers.h"

#include "core.h"
#include "native_dispatch.h"
#include "render/actor_producers.h"
#include "render/gte_words.h"
#include "runtime/toystory2_context.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace ts2 {
namespace {

constexpr std::uint32_t kScratch = 0x1F800000u;         // s0..s7 are spilled here
constexpr std::uint32_t kLight = kScratch + 0x20u;      // 0x8001CD34: a1..a3, IR1..IR3 for DCPL
constexpr std::uint32_t kNormalBase = kScratch + 0x20u; // 0x8001DFE4: a3, the colour normals are lit on
constexpr std::uint32_t kSavedBackground = kScratch + 0x24u;
constexpr std::uint32_t kSavedFacing = kScratch + 0xE0u;
constexpr std::uint32_t kOrderingTable = 0x800A10BCu; // base the bucket offsets are added to
constexpr std::uint32_t kVisibleRows = 0x800A1100u;   // >= 0: rows [0, v); < 0: rows [v, 240)
constexpr std::uint32_t kPacketCursor = 0x800A1608u;
constexpr std::uint32_t kFacing = 0x800A13B4u;        // sign flips which winding faces the camera
constexpr std::uint32_t kBucketOffsets = 0x800A12F8u; // u16 per OTZ (index OTZ * 4)
constexpr std::uint32_t kDepthLimit = 0x800A135Cu;    // OTZ limit; the drawers scale it by 4
constexpr std::uint32_t kTexturePages = 0x800CD200u;  // 16 bytes per page: tpage, clut base
constexpr std::uint32_t kPartNormals = 0x0Cu;         // u16 count, normals of 8 bytes, u16 index per corner
constexpr std::uint32_t kPartFaces = 0x20u;

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
  Core &core;
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
Walk begin(Core &core) {
  for (std::uint32_t s = 0; s != 8u; ++s) {
    core.mem_w32(kScratch + s * 4u, core.r[16u + s]);
  }
  const std::uint32_t part = core.r[4];
  const std::uint32_t faces = core.mem_r32(part + kPartFaces);
  const auto rows = static_cast<std::int32_t>(core.mem_r32(kVisibleRows));
  Walk walk{.core = core,
            .part = part,
            .face = faces,
            .ot = core.mem_r32(kOrderingTable),
            .packet = core.mem_r32(kPacketCursor),
            .top = rows < 0 ? static_cast<std::int32_t>(static_cast<std::uint32_t>(rows) << 16) : 0,
            .bottom = rows < 0 ? 0x00F00000 : static_cast<std::int32_t>(static_cast<std::uint32_t>(rows) << 16),
            .facing = core.mem_r32(kFacing),
            .buckets = core.mem_r32(kBucketOffsets),
            .depthLimit = core.mem_r32(kDepthLimit) << 2,
            .tpage = 0,
            .clutBase = 0,
            .normals = 0,
            .corners = 0};
  const bool quad = (core.mem_r32(faces) & kKindMask) == kKindMask;
  const auto select = static_cast<std::uint32_t>(core.mem_r8s(faces + (quad ? 0x2Fu : 0x23u)));
  const std::uint32_t page = kTexturePages + ((select & 0x1Fu) << 4);
  walk.clutBase = core.mem_r16(page + 2u);
  walk.tpage = core.mem_r16(page) + (select & 0x60u);
  return walk;
}

void end(Walk &walk) {
  walk.core.mem_w32(kPacketCursor, walk.packet);
  walk.core.r[2] = kScratch;
}

// SZ1 in front of the near plane and within the depth limit.
bool nearEnough(const Walk &walk) {
  const auto sz1 = static_cast<std::int32_t>(gteRead(gte::kSz1));
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
  return walk.core.mem_r16(walk.buckets + (otz << 2)) + walk.ot;
}

void insert(Walk &walk, std::uint32_t at, std::uint32_t length, std::uint32_t bytes, std::uint32_t uv0) {
  Core &core = walk.core;
  const std::uint32_t next = core.mem_r32(at);
  core.mem_w32(at, walk.packet & 0x00FFFFFFu);
  core.mem_w32(walk.packet, next + length);
  core.mem_w16(walk.packet + 0x1Au, static_cast<std::uint16_t>(walk.tpage));
  core.mem_w16(walk.packet + 0x0Eu,
               static_cast<std::uint16_t>(((uv0 >> 12) & 0xCu) + ((uv0 >> 6) & 3u) + walk.clutBase));
  walk.packet += bytes;
}

void storeColours(Core &core, std::uint32_t packet, std::span<const std::uint32_t> colours) {
  for (std::size_t i = 1; i != colours.size(); ++i) {
    core.mem_w32(packet + 4u + static_cast<std::uint32_t>(i) * 0x0Cu, colours[i]);
  }
}

// Quad record: +0 colour 0 and type, +4/+C/+1C/+14 xy of vertices 0..3, then each vertex's z/uv word,
// +24 colour 1, +28 colour 3, +2C colour 2 (negative: two-sided). Only the prelit drawer stores uv0 early.
std::optional<Projected> projectQuad(Walk &walk, std::uint32_t head, bool storeUv0) {
  Core &core = walk.core;
  const std::uint32_t f = walk.face;
  const std::uint32_t p = walk.packet;
  gteWrite(gte::kVxy1, core.mem_r32(f + 0x0Cu));
  gteWrite(gte::kVxy2, core.mem_r32(f + 0x1Cu));
  const std::uint32_t zuv0 = core.mem_r32(f + 0x08u);
  const std::uint32_t zuv1 = core.mem_r32(f + 0x10u);
  const std::uint32_t zuv2 = core.mem_r32(f + 0x20u);
  gteWrite(gte::kVz0, zuv0);
  gteWrite(gte::kVz1, zuv1);
  gteWrite(gte::kVz2, zuv2);
  gte_op(&core, gte::kRtpt);
  const std::uint32_t xy3 = core.mem_r32(f + 0x14u);
  const std::uint32_t zuv3 = core.mem_r32(f + 0x18u);
  if (storeUv0) {
    core.mem_w16(p + 0x0Cu, static_cast<std::uint16_t>(zuv0 >> 16));
  }
  core.mem_w16(p + 0x18u, static_cast<std::uint16_t>(zuv1 >> 16));
  core.mem_w16(p + 0x24u, static_cast<std::uint16_t>(zuv2 >> 16));
  core.mem_w16(p + 0x30u, static_cast<std::uint16_t>(zuv3 >> 16));
  if (!nearEnough(walk)) {
    return std::nullopt;
  }
  const std::uint32_t xy0 = gteRead(gte::kSxy0);
  const std::uint32_t xy1 = gteRead(gte::kSxy1);
  const std::uint32_t xy2 = gteRead(gte::kSxy2);
  gte_op(&core, gte::kNclip);
  gteWrite(gte::kVxy0, xy3);
  gteWrite(gte::kVz0, zuv3);
  const std::uint32_t front = gteRead(gte::kMac0) ^ walk.facing;
  gte_op(&core, gte::kRtps);
  const std::uint32_t colour2 = core.mem_r32(f + 0x2Cu);
  const std::uint32_t colour1 = core.mem_r32(f + 0x24u);
  const std::uint32_t colour3 = core.mem_r32(f + 0x28u);
  if (static_cast<std::int32_t>(colour2) >= 0) {
    gte_op(&core, gte::kNclip);
    if (static_cast<std::int32_t>(front) < 0 && static_cast<std::int32_t>(gteRead(gte::kMac0) ^ walk.facing) > 0) {
      return std::nullopt;
    }
  }
  const std::uint32_t screen3 = gteRead(gte::kSxy2);
  gte_op(&core, gte::kAvsz4);
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
  Core &core = walk.core;
  const std::uint32_t f = walk.face;
  const std::uint32_t p = walk.packet;
  gteWrite(gte::kVxy2, core.mem_r32(f + 0x14u));
  const std::uint32_t zuv0 = core.mem_r32(f + 0x08u);
  const std::uint32_t zuv1 = core.mem_r32(f + 0x10u);
  const std::uint32_t zuv2 = core.mem_r32(f + 0x18u);
  gteWrite(gte::kVz0, zuv0);
  gteWrite(gte::kVz1, zuv1);
  gteWrite(gte::kVz2, zuv2);
  gte_op(&core, gte::kRtpt);
  const std::uint32_t colour1 = core.mem_r32(f + 0x1Cu);
  const std::uint32_t colour2 = core.mem_r32(f + 0x20u);
  core.mem_w16(p + 0x0Cu, static_cast<std::uint16_t>(zuv0 >> 16));
  core.mem_w16(p + 0x18u, static_cast<std::uint16_t>(zuv1 >> 16));
  core.mem_w16(p + 0x24u, static_cast<std::uint16_t>(zuv2 >> 16));
  if (!nearEnough(walk)) {
    return std::nullopt;
  }
  const std::uint32_t xy0 = gteRead(gte::kSxy0);
  const std::uint32_t xy1 = gteRead(gte::kSxy1);
  const std::uint32_t xy2 = gteRead(gte::kSxy2);
  gte_op(&core, gte::kNclip);
  const std::uint32_t front = gteRead(gte::kMac0) ^ walk.facing;
  gte_op(&core, gte::kAvsz3);
  if (static_cast<std::int32_t>(colour2) >= 0 && static_cast<std::int32_t>(front) < 0) {
    return std::nullopt;
  }
  Projected face{
      .xy = {xy0, xy1, xy2, 0u}, .colours = {head, colour1, colour2, 0u}, .uv0 = zuv0 >> 16, .rows = Rows::Inside};
  face.rows = rows(walk, std::span(face.xy).first(kTriangleCorners));
  return face;
}

void prelitQuad(Walk &walk, std::uint32_t head) {
  Core &core = walk.core;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectQuad(walk, head, true);
  if (!face) {
    return;
  }
  if (face->rows != Rows::Above) {
    core.mem_w32(p + 0x08u, face->xy[0]);
  }
  if (face->rows != Rows::Inside) {
    return;
  }
  core.mem_w32(p + 0x2Cu, face->xy[3]);
  const std::uint32_t otz = gteRead(gte::kOtz);
  core.mem_w32(p + 0x14u, face->xy[1]);
  core.mem_w32(p + 0x20u, face->xy[2]);
  const std::uint32_t at = bucket(walk, otz);
  if (at == 0u) {
    return;
  }
  core.mem_w32(p + 0x04u, head);
  storeColours(core, p, face->colours);
  insert(walk, at, kGt4Length, kGt4Bytes, face->uv0);
}

void prelitTriangle(Walk &walk, std::uint32_t head) {
  Core &core = walk.core;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectTriangle(walk, head);
  if (!face) {
    return;
  }
  if (face->rows != Rows::Above) {
    core.mem_w32(p + 0x04u, head);
  }
  if (face->rows != Rows::Inside) {
    return;
  }
  storeColours(core, p, std::span(face->colours).first(kTriangleCorners));
  core.mem_w32(p + 0x08u, face->xy[0]);
  const std::uint32_t otz = gteRead(gte::kOtz);
  core.mem_w32(p + 0x14u, face->xy[1]);
  core.mem_w32(p + 0x20u, face->xy[2]);
  const std::uint32_t at = bucket(walk, otz);
  if (at == 0u) {
    return;
  }
  insert(walk, at, kGt3Length, kGt3Bytes, face->uv0);
}

// DCPL each colour with the lit drawer's light, storing each result one step behind.
void light(Walk &walk, std::span<const std::uint32_t> colours, std::uint32_t packet) {
  Core &core = walk.core;
  const std::array<std::uint32_t, 3> ir{core.mem_r32(kLight), core.mem_r32(kLight + 4u), core.mem_r32(kLight + 8u)};
  for (std::size_t i = 0; i != colours.size(); ++i) {
    gteWrite(gte::kIr1, ir[0]);
    gteWrite(gte::kIr2, ir[1]);
    gteWrite(gte::kIr3, ir[2]);
    gteWrite(gte::kRgbc, colours[i]);
    if (i == 0) {
      gteWrite(gte::kIr0, 0u);
    } else {
      core.mem_w32(packet + 4u + static_cast<std::uint32_t>(i - 1) * 0x0Cu, gteRead(gte::kRgb2));
    }
    gte_op(&core, gte::kDcpl);
  }
  core.mem_w32(packet + 4u + static_cast<std::uint32_t>(colours.size() - 1) * 0x0Cu, gteRead(gte::kRgb2));
}

void litQuad(Walk &walk, std::uint32_t head) {
  Core &core = walk.core;
  const std::uint32_t f = walk.face;
  const std::uint32_t p = walk.packet;
  gteWrite(gte::kVz0, core.mem_r32(f + 0x08u));
  gteWrite(gte::kVxy1, core.mem_r32(f + 0x0Cu));
  gteWrite(gte::kVz1, core.mem_r32(f + 0x10u));
  gteWrite(gte::kVxy2, core.mem_r32(f + 0x1Cu));
  gteWrite(gte::kVz2, core.mem_r32(f + 0x20u));
  const std::uint32_t xy3 = core.mem_r32(f + 0x14u);
  gte_op(&core, gte::kRtpt);
  const std::uint32_t zuv3 = core.mem_r32(f + 0x18u);
  const std::uint16_t uv1 = core.mem_r16(f + 0x12u);
  const std::uint16_t uv3 = core.mem_r16(f + 0x1Au);
  const std::uint16_t uv2 = core.mem_r16(f + 0x22u);
  core.mem_w16(p + 0x18u, uv1);
  if (!nearEnough(walk)) {
    return;
  }
  const std::uint32_t xy0 = gteRead(gte::kSxy0);
  const std::uint32_t xy1 = gteRead(gte::kSxy1);
  const std::uint32_t xy2 = gteRead(gte::kSxy2);
  gte_op(&core, gte::kNclip);
  core.mem_w16(p + 0x24u, uv2);
  core.mem_w16(p + 0x30u, uv3);
  gteWrite(gte::kVxy0, xy3);
  gteWrite(gte::kVz0, zuv3);
  const std::uint32_t front = gteRead(gte::kMac0);
  gte_op(&core, gte::kRtps);
  const std::uint16_t uv0 = core.mem_r16(f + 0x0Au);
  const std::uint32_t colour2 = core.mem_r32(f + 0x2Cu);
  const std::uint32_t colour1 = core.mem_r32(f + 0x24u);
  const std::uint32_t colour3 = core.mem_r32(f + 0x28u);
  if (static_cast<std::int32_t>(colour2) >= 0) {
    gte_op(&core, gte::kNclip);
    // The second winding is tested without the facing sign, as the guest does.
    if (static_cast<std::int32_t>(front ^ walk.facing) < 0 && static_cast<std::int32_t>(gteRead(gte::kMac0)) > 0) {
      return;
    }
  }
  const std::uint32_t screen3 = gteRead(gte::kSxy2);
  gte_op(&core, gte::kAvsz4);
  const std::array<std::uint32_t, 4> xy{xy0, xy1, xy2, screen3};
  const Rows inRows = rows(walk, xy);
  if (inRows != Rows::Above) {
    core.mem_w16(p + 0x0Cu, uv0);
  }
  if (inRows != Rows::Inside) {
    return;
  }
  core.mem_w32(p + 0x08u, xy0);
  core.mem_w32(p + 0x2Cu, screen3);
  const std::array<std::uint32_t, 4> colours{head, colour1, colour2, colour3};
  light(walk, colours, p);
  const std::uint32_t otz = gteRead(gte::kOtz);
  core.mem_w32(p + 0x14u, xy1);
  core.mem_w32(p + 0x20u, xy2);
  const std::uint32_t at = bucket(walk, otz);
  if (at == 0u) {
    return;
  }
  insert(walk, at, kGt4Length, kGt4Bytes, uv0);
}

void litTriangle(Walk &walk, std::uint32_t head) {
  Core &core = walk.core;
  const std::uint32_t f = walk.face;
  const std::uint32_t p = walk.packet;
  gteWrite(gte::kVxy1, core.mem_r32(f + 0x0Cu));
  gteWrite(gte::kVz1, core.mem_r32(f + 0x10u));
  gteWrite(gte::kVxy2, core.mem_r32(f + 0x14u));
  gteWrite(gte::kVz2, core.mem_r32(f + 0x18u));
  gte_op(&core, gte::kRtpt);
  const std::uint16_t uv1 = core.mem_r16(f + 0x12u);
  const std::uint16_t uv2 = core.mem_r16(f + 0x1Au);
  core.mem_w16(p + 0x18u, uv1);
  if (!nearEnough(walk)) {
    return;
  }
  const std::uint32_t xy0 = gteRead(gte::kSxy0);
  const std::uint32_t xy1 = gteRead(gte::kSxy1);
  const std::uint32_t xy2 = gteRead(gte::kSxy2);
  gte_op(&core, gte::kNclip);
  core.mem_w16(p + 0x24u, uv2);
  const std::uint32_t front = gteRead(gte::kMac0) ^ walk.facing;
  const std::uint16_t uv0 = core.mem_r16(f + 0x0Au);
  const std::uint32_t colour1 = core.mem_r32(f + 0x1Cu);
  const std::uint32_t colour2 = core.mem_r32(f + 0x20u);
  gte_op(&core, gte::kAvsz3);
  if (static_cast<std::int32_t>(colour2) >= 0 && static_cast<std::int32_t>(front) < 0) {
    return;
  }
  const std::array<std::uint32_t, 3> xy{xy0, xy1, xy2};
  const Rows inRows = rows(walk, xy);
  if (inRows != Rows::Above) {
    core.mem_w16(p + 0x0Cu, uv0);
  }
  if (inRows != Rows::Inside) {
    return;
  }
  core.mem_w32(p + 0x08u, xy0);
  const std::array<std::uint32_t, 3> colours{head, colour1, colour2};
  light(walk, colours, p);
  const std::uint32_t otz = gteRead(gte::kOtz);
  core.mem_w32(p + 0x14u, xy1);
  core.mem_w32(p + 0x20u, xy2);
  const std::uint32_t at = bucket(walk, otz);
  if (at == 0u) {
    return;
  }
  insert(walk, at, kGt3Length, kGt3Bytes, uv0);
}

// The corner index slot each packet corner is lit from: a quad's packet corners 2 and 3 are its
// record's vertices 3 and 2.
constexpr std::array<std::uint32_t, 4> kQuadNormalSlots{0u, 1u, 3u, 2u};
constexpr std::array<std::uint32_t, 3> kTriangleNormalSlots{0u, 1u, 2u};

std::array<std::uint32_t, 3> normal(const Walk &walk, std::uint32_t slot) {
  Core &core = walk.core;
  const std::uint32_t at = walk.normals + (static_cast<std::uint32_t>(core.mem_r16(walk.corners + slot * 2u)) << 3);
  return {static_cast<std::uint32_t>(core.mem_r16s(at)),
          static_cast<std::uint32_t>(core.mem_r16s(at + 2u)),
          static_cast<std::uint32_t>(core.mem_r16s(at + 4u))};
}

void setIr(const std::array<std::uint32_t, 3> &ir) {
  gteWrite(gte::kIr1, ir[0]);
  gteWrite(gte::kIr2, ir[1]);
  gteWrite(gte::kIr3, ir[2]);
}

// 0x8001DFE4 lights each corner with CC then DCPL, storing each result one step behind; the last is stored
// only once the face has a bucket.
void lightCorners(Walk &walk,
                  std::uint32_t base,
                  std::span<const std::uint32_t> colours,
                  std::span<const std::uint32_t> slots) {
  Core &core = walk.core;
  setIr(normal(walk, slots[0]));
  gte_op(&core, gte::kCc);
  for (std::size_t i = 0; i != colours.size(); ++i) {
    setIr({(gteRead(gte::kMac1) << 1) + 0x1000u,
           (gteRead(gte::kMac2) << 1) + 0x1000u,
           (gteRead(gte::kMac3) << 1) + 0x1000u});
    gteWrite(gte::kRgbc, colours[i]);
    gteWrite(gte::kIr0, 0u);
    gte_op(&core, gte::kDcpl);
    if (i + 1 == colours.size()) {
      return;
    }
    gteWrite(gte::kRgbc, base);
    setIr(normal(walk, slots[i + 1]));
    core.mem_w32(walk.packet + 4u + static_cast<std::uint32_t>(i) * 0x0Cu, gteRead(gte::kRgb2));
    gte_op(&core, gte::kCc);
  }
}

// A negative first corner index draws the face's own colours unlit.
void normalLitQuad(Walk &walk, std::uint32_t head) {
  Core &core = walk.core;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectQuad(walk, head, false);
  if (!face || face->rows != Rows::Inside) {
    return;
  }
  const std::uint32_t base = core.mem_r32(kNormalBase);
  const bool unlit = core.mem_r16s(walk.corners) < 0;
  core.mem_w32(p + 0x08u, face->xy[0]);
  core.mem_w32(p + 0x14u, face->xy[1]);
  core.mem_w32(p + 0x20u, face->xy[2]);
  gteWrite(gte::kRgbc, base);
  core.mem_w16(p + 0x0Cu, static_cast<std::uint16_t>(face->uv0));
  core.mem_w32(p + 0x2Cu, face->xy[3]);
  if (unlit) {
    core.mem_w32(p + 0x04u, head);
  } else {
    lightCorners(walk, base, face->colours, kQuadNormalSlots);
  }
  const std::uint32_t at = bucket(walk, gteRead(gte::kOtz));
  if (at == 0u) {
    return;
  }
  if (unlit) {
    storeColours(core, p, face->colours);
  } else {
    core.mem_w32(p + 0x28u, gteRead(gte::kRgb2));
  }
  insert(walk, at, kGt4Length, kGt4Bytes, face->uv0);
}

void normalLitTriangle(Walk &walk, std::uint32_t head) {
  Core &core = walk.core;
  const std::uint32_t p = walk.packet;
  const std::optional<Projected> face = projectTriangle(walk, head);
  if (!face) {
    return;
  }
  if (face->rows != Rows::Above) {
    core.mem_w32(p + 0x08u, face->xy[0]);
  }
  if (face->rows != Rows::Inside) {
    return;
  }
  core.mem_w32(p + 0x14u, face->xy[1]);
  core.mem_w32(p + 0x20u, face->xy[2]);
  const std::uint32_t base = core.mem_r32(kNormalBase);
  const bool unlit = core.mem_r16s(walk.corners) < 0;
  gteWrite(gte::kRgbc, base);
  const std::span<const std::uint32_t> colours = std::span(face->colours).first(kTriangleCorners);
  if (unlit) {
    core.mem_w32(p + 0x04u, head);
  } else {
    lightCorners(walk, base, colours, kTriangleNormalSlots);
  }
  const std::uint32_t at = bucket(walk, gteRead(gte::kOtz));
  if (at == 0u) {
    return;
  }
  if (unlit) {
    storeColours(core, p, colours);
  } else {
    core.mem_w32(p + 0x1Cu, gteRead(gte::kRgb2));
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
  Core &core = walk.core;
  for (std::uint32_t index = 0;; ++index) {
    const std::uint32_t head = core.mem_r32(walk.face);
    gteWrite(gte::kVxy0, core.mem_r32(walk.face + 4u));
    const std::uint32_t kind = head & kKindMask;
    const bool isQuad = kind == kKindMask;
    if (!isQuad) {
      gteWrite(lead.reg, core.mem_r32(walk.face + lead.offset));
    }
    if (kind == 0u) {
      break;
    }
    if (const std::optional<std::uint32_t> actor = context(core).actorProducers.drawing()) {
      const psx::present::EmissionScope::Guard key(
          core.emission, ActorProducers::kActorRenderer, *actor, ActorProducers::faceElement(walk.part, index));
      (isQuad ? quad : triangle)(walk, head);
    } else {
      (isQuad ? quad : triangle)(walk, head);
    }
    walk.face += isQuad ? kQuadBytes : kTriangleBytes;
    walk.corners += (isQuad ? kQuadCorners : kTriangleCorners) * 2u;
  }
  end(walk);
}

void drawPrelit(Core *core) {
  Walk walk = begin(*core);
  walkFaces(walk, &prelitQuad, &prelitTriangle, TriangleLead{gte::kVxy1, 0x0Cu});
}

void drawLit(Core *core) {
  core->mem_w32(kLight, core->r[5]);
  core->mem_w32(kLight + 4u, core->r[6]);
  core->mem_w32(kLight + 8u, core->r[7]);
  Walk walk = begin(*core);
  walkFaces(walk, &litQuad, &litTriangle, TriangleLead{gte::kVz0, 0x08u});
}

// a1 points at the light vector (three words, low halves used), a2 is the background colour for the
// walk, a3 the base colour.
void drawNormalLit(Core *core) {
  Core &c = *core;
  const std::uint32_t lx = c.mem_r16(c.r[5]);
  const std::uint32_t ly = c.mem_r16(c.r[5] + 4u);
  const std::uint32_t lz = c.mem_r16(c.r[5] + 8u);
  const std::uint32_t xy = (ly << 16) + lx;
  const std::uint32_t zx = (lx << 16) + lz;
  gte_write_ctrl(kLightColour, xy);
  gte_write_ctrl(kLightColour + 1u, zx);
  gte_write_ctrl(kLightColour + 2u, (lz << 16) + ly);
  gte_write_ctrl(kLightColour + 3u, xy);
  gte_write_ctrl(kLightColour + 4u, zx);
  c.mem_w32(kNormalBase, c.r[7]);
  for (std::uint32_t i = 0; i != 3u; ++i) {
    c.mem_w32(kSavedBackground + i * 4u, gte_read_ctrl(kBackground + i));
    gte_write_ctrl(kBackground + i, c.r[6]);
  }
  const std::uint32_t normals = c.mem_r32(c.r[4] + kPartNormals);
  Walk walk = begin(c);
  c.mem_w32(kSavedFacing, walk.facing);
  walk.normals = normals + 4u;
  walk.corners = walk.normals + (static_cast<std::uint32_t>(c.mem_r16(normals)) << 3);
  walkFaces(walk, &normalLitQuad, &normalLitTriangle, TriangleLead{gte::kVxy1, 0x0Cu});
  for (std::uint32_t i = 0; i != 3u; ++i) {
    gte_write_ctrl(kBackground + i, c.mem_r32(kSavedBackground + i * 4u));
  }
}

} // namespace

void PartFaceDrawers::install(Core &core) {
  psx::cpu::installNativeOverride(core, kPrelit, "prelit-part-drawer", &drawPrelit);
  psx::cpu::installNativeOverride(core, kLit, "lit-part-drawer", &drawLit);
  psx::cpu::installNativeOverride(core, kNormalLit, "normal-lit-part-drawer", &drawNormalLit);
}

} // namespace ts2
