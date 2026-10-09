#include "render/slot_packets.h"

#include "render/mesh_culling.h"
#include "render/mesh_scratch.h"

namespace ts2 {

using psx::present::EmitMemory;

namespace {

using namespace mesh_scratch;

constexpr std::uint32_t kQuadLength = 0x0C000000u;
constexpr std::uint32_t kPlainQuadLength = 0x08000000u;
constexpr std::uint32_t kTriangleLength = 0x09000000u;
constexpr std::uint32_t kPlainTriangleLength = 0x06000000u;
constexpr std::uint32_t kPlainBias = 0x04000000u; // GT4 -> G4 and GT3 -> G3 command bytes

} // namespace

std::uint32_t setTexturePage(const EmitMemory &memory, std::int32_t word) {
  const auto bits = static_cast<std::uint32_t>(word);
  const std::uint32_t mode = bits & 0x60u;
  const std::uint32_t select = (bits >> 4) & 0x1F0u;
  const std::uint32_t entry = kTexturePages + select;
  const std::uint32_t clutWord = memory.mem_r32(entry);
  const std::uint32_t pageLow = clutWord & 0xFFFFu;
  memory.mem_w32(kClutHigh, clutWord ^ pageLow);
  memory.mem_w16(kClutMode, memory.mem_r16(entry + 8u));
  memory.mem_w16(kPageLow, static_cast<std::uint16_t>(pageLow + mode));
  const std::uint32_t semi = (mode + 0x20u) & 0x60u;
  memory.mem_w32(kSemiMode, semi);
  memory.mem_w32(kQuadCommand, semi == 0u ? 0x3C000000u : 0x3E000000u);
  memory.mem_w32(kTriangleCommand, semi == 0u ? 0x34000000u : 0x36000000u);
  return select;
}

SlotPair fillNewSlot(const EmitMemory &m,
                     PacketShape shape,
                     std::uint32_t descriptor,
                     const std::array<std::uint32_t, 4> &corners,
                     std::uint32_t slot,
                     std::uint32_t packets) {
  const std::uint32_t words = slot << 2;
  const SlotPair pair{m.mem_r32(kAltPackets) + words, packets + words};
  const auto both32 = [&](std::uint32_t offset, std::uint32_t value) {
    m.mem_w32(pair.alt + offset, value);
    m.mem_w32(pair.own + offset, value);
  };
  const auto both16 = [&](std::uint32_t offset, std::uint16_t value) {
    m.mem_w16(pair.alt + offset, value);
    m.mem_w16(pair.own + offset, value);
  };
  std::array<std::uint32_t, 4> colour{};
  for (std::size_t i = 0; i != (shape.quad ? 4u : 3u); ++i) {
    colour[i] = packColour(m.mem_r16s(corners[i] + 6u), i == 0);
  }
  const std::uint32_t command = m.mem_r32(shape.quad ? kQuadCommand : kTriangleCommand);
  if (!shape.textured) {
    both32(12, colour[1]);
    both32(4, colour[0] + command - kPlainBias);
    if (shape.quad) {
      both32(20, colour[3]);
      both32(28, colour[2]);
    } else {
      both32(20, colour[2]);
    }
    both16(38, 0);
    return pair;
  }
  both32(16, colour[1]);
  both32(4, colour[0] + command);
  if (shape.quad) {
    both32(28, colour[3]);
    both32(40, colour[2]);
  } else {
    both32(28, colour[2]);
  }
  std::uint32_t clut = m.mem_r32(kClutHigh);
  const std::uint32_t uv = shape.quad ? m.mem_r32(descriptor + 4u) : m.mem_r16(descriptor + 6u);
  const std::uint32_t rest = m.mem_r32(descriptor + 8u);
  if (m.mem_r16(kClutMode) == 0u) {
    clut += (((uv >> 12) & 0xCu) + ((uv >> 6) & 3u)) << 16;
  }
  const std::uint32_t first = (shape.quad ? (uv & 0xFFFFu) : uv) + clut;
  const std::uint32_t page = m.mem_r32(kPageWord);
  both32(12, first);
  if (shape.quad) {
    both32(24, (uv >> 16) + page);
    both16(36, static_cast<std::uint16_t>(rest >> 16));
    both16(48, static_cast<std::uint16_t>(rest));
  } else {
    both32(24, (rest & 0xFFFFu) + page);
    both16(36, static_cast<std::uint16_t>(rest >> 16));
  }
  both16(38, 0);
  return pair;
}

void bindPacket(const EmitMemory &memory, std::uint32_t packet) {
  if (!memory.hosted() && memory.core().emission.isOpen()) {
    memory.core().emission.bindPacket(packet);
  }
}

void linkPacket(const EmitMemory &m,
                PacketShape shape,
                std::uint32_t packet,
                std::uint32_t head,
                const std::array<std::uint32_t, 4> &xy) {
  const std::uint32_t chained = m.mem_r32(head);
  const std::uint32_t step = shape.textured ? 12u : 8u;
  m.mem_w32(packet + 8u, xy[0]);
  m.mem_w32(packet + 8u + step, xy[1]);
  m.mem_w32(packet + 8u + 2u * step, xy[2]);
  if (shape.quad) {
    m.mem_w32(packet + 8u + 3u * step, xy[3]);
  }
  std::uint32_t length = shape.textured ? (shape.quad ? kQuadLength : kTriangleLength)
                                        : (shape.quad ? kPlainQuadLength : kPlainTriangleLength);
  m.mem_w32(head, (packet << 8) >> 8);
  m.mem_w32(packet, chained + length);
  bindPacket(m, packet);
}

} // namespace ts2
