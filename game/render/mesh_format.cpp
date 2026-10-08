#include "render/mesh_format.h"

#include "core.h"

#include <cstdint>
#include <limits>

namespace ts2 {
namespace {

constexpr uint32_t kGuestRamSize = 2u * 1024u * 1024u;
constexpr uint32_t kVertexStride = 8u;

bool guestRamRange(uint32_t address, uint64_t byteCount) {
  const uint32_t segment = address & 0xE0000000u;
  if (segment != 0 && segment != 0x80000000u && segment != 0xA0000000u) {
    return false;
  }
  const uint32_t physical = address & 0x1FFFFFFFu;
  return physical < kGuestRamSize && byteCount <= kGuestRamSize - physical;
}

std::optional<uint32_t> checkedResidentAddress(uint32_t base, uint64_t offset, uint32_t byteCount) {
  if (offset > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  const uint64_t physical = static_cast<uint64_t>(base & 0x1FFFFFFFu) + offset;
  if (physical > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  const uint32_t address = base + static_cast<uint32_t>(offset);
  return guestRamRange(address, byteCount) ? std::optional<uint32_t>{address} : std::nullopt;
}

} // namespace

std::optional<ResidentMeshLayout> decodeResidentMeshLayout(Core &core, uint32_t meshAddress) {
  if (!guestRamRange(meshAddress, sizeof(uint32_t))) {
    return std::nullopt;
  }
  const int32_t headerWord = static_cast<int32_t>(core.mem_r32(meshAddress));
  if (headerWord == std::numeric_limits<int32_t>::min()) {
    return std::nullopt;
  }
  const uint32_t vertexCount = headerWord > 0 ? static_cast<uint32_t>(headerWord) : static_cast<uint32_t>(-headerWord);
  const bool hasAuxiliaryVertexRecords = headerWord <= 0;
  const uint64_t commandOffset = hasAuxiliaryVertexRecords ? 8u + static_cast<uint64_t>(vertexCount) * 12u
                                                           : 4u + static_cast<uint64_t>(vertexCount) * kVertexStride;
  const std::optional<uint32_t> vertexAddress = checkedResidentAddress(meshAddress, 4u, kVertexStride);
  const std::optional<uint32_t> commandAddress = checkedResidentAddress(meshAddress, commandOffset, sizeof(uint32_t));
  if (!vertexAddress || !commandAddress) {
    return std::nullopt;
  }
  return ResidentMeshLayout{
      .meshAddress = meshAddress,
      .vertexAddress = *vertexAddress,
      .commandAddress = *commandAddress,
      .headerWord = headerWord,
      .vertexCount = vertexCount,
      .hasAuxiliaryVertexRecords = hasAuxiliaryVertexRecords,
  };
}

std::optional<ResidentMeshCommand> decodeResidentMeshCommand(Core &core, uint32_t commandAddress) {
  if (!guestRamRange(commandAddress, sizeof(uint32_t))) {
    return std::nullopt;
  }
  const uint8_t opcode = static_cast<uint8_t>(core.mem_r16(commandAddress) & 0x1Fu);
  const int16_t primitiveCount = core.mem_r16s(commandAddress + 2u);
  const bool terminal = opcode >= 24u;
  if (terminal) {
    return ResidentMeshCommand{
        .address = commandAddress,
        .nextCommandAddress = commandAddress + 4u,
        .opcode = opcode,
        .terminal = true,
    };
  }
  if (primitiveCount <= 0) {
    return std::nullopt;
  }
  const uint32_t descriptorStride = opcode < 16u ? 12u : 4u;
  const uint64_t descriptorBytes = static_cast<uint64_t>(primitiveCount) * descriptorStride;
  const std::optional<uint32_t> nextCommandAddress =
      checkedResidentAddress(commandAddress, sizeof(uint32_t) + descriptorBytes, sizeof(uint32_t));
  if (!nextCommandAddress) {
    return std::nullopt;
  }
  return ResidentMeshCommand{
      .address = commandAddress,
      .nextCommandAddress = *nextCommandAddress,
      .primitiveCount = static_cast<uint16_t>(primitiveCount),
      .opcode = opcode,
  };
}

} // namespace ts2
