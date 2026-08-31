#include "render/resident_mesh_format.h"

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

ResidentMeshMaterialState readMaterialState(Core &core, uint16_t tableOffset, uint16_t blendBits) {
  const uint32_t tableAddress = kResidentMaterialTableBase + tableOffset;
  return ResidentMeshMaterialState{
      .tableOffset = tableOffset,
      .blendBits = blendBits,
      .tableAddress = tableAddress,
      .tableWord0 = core.mem_r32(tableAddress),
      .tableWord8 = core.mem_r16(tableAddress + 8u),
  };
}

ResidentMeshDescriptorSample makeDescriptorSample(Core &core,
                                                  const ResidentMeshCommand &command,
                                                  uint32_t primitiveIndex,
                                                  const ResidentMeshPrimitive &primitive,
                                                  const ResidentMeshMaterialState &material) {
  ResidentMeshDescriptorSample sample{
      .commandAddress = command.address,
      .descriptorAddress = primitive.address,
      .commandWord = static_cast<uint16_t>(command.word),
      .primitiveIndex = static_cast<uint16_t>(primitiveIndex),
      .material = material,
      .descriptorWords = {0u, 0u, 0u},
  };
  for (uint32_t word = 0; word < command.descriptorStride / sizeof(uint32_t); ++word) {
    sample.descriptorWords[word] = core.mem_r32(primitive.address + word * sizeof(uint32_t));
  }
  return sample;
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

std::optional<ResidentMeshVertex>
decodeResidentMeshVertex(Core &core, const ResidentMeshLayout &layout, uint32_t vertexIndex) {
  if (vertexIndex >= layout.vertexCount) {
    return std::nullopt;
  }
  const std::optional<uint32_t> address =
      checkedResidentAddress(layout.vertexAddress, static_cast<uint64_t>(vertexIndex) * kVertexStride, kVertexStride);
  if (!address) {
    return std::nullopt;
  }
  return ResidentMeshVertex{
      .address = *address,
      .x = static_cast<int16_t>(core.mem_r16s(*address)),
      .y = static_cast<int16_t>(core.mem_r16s(*address + 2u)),
      .z = static_cast<int16_t>(core.mem_r16s(*address + 4u)),
      .color555 = core.mem_r16(*address + 6u),
  };
}

std::optional<ResidentMeshCommand> decodeResidentMeshCommand(Core &core, uint32_t commandAddress) {
  if (!guestRamRange(commandAddress, sizeof(uint32_t))) {
    return std::nullopt;
  }
  const int16_t word = core.mem_r16s(commandAddress);
  const int16_t primitiveCount = core.mem_r16s(commandAddress + 2u);
  const uint16_t encodedWord = static_cast<uint16_t>(word);
  const uint8_t opcode = static_cast<uint8_t>(encodedWord & 0x1Fu);
  const bool terminal = opcode >= 24u;
  if (!terminal && primitiveCount <= 0) {
    return std::nullopt;
  }
  const uint8_t descriptorStride = terminal ? 0u : (opcode < 16u ? 12u : 4u);
  const uint8_t vertexCount = terminal ? 0u : ((opcode & 1u) == 0 ? 4u : 3u);
  const uint64_t descriptorBytes =
      terminal ? 0u : static_cast<uint64_t>(static_cast<uint16_t>(primitiveCount)) * descriptorStride;
  const std::optional<uint32_t> descriptorAddress =
      terminal ? std::optional<uint32_t>{commandAddress + sizeof(uint32_t)}
               : checkedResidentAddress(commandAddress, sizeof(uint32_t), descriptorStride);
  const std::optional<uint32_t> nextCommandAddress =
      terminal ? std::optional<uint32_t>{commandAddress + sizeof(uint32_t)}
               : checkedResidentAddress(commandAddress, sizeof(uint32_t) + descriptorBytes, sizeof(uint32_t));
  if (!descriptorAddress || !nextCommandAddress) {
    return std::nullopt;
  }
  return ResidentMeshCommand{
      .address = commandAddress,
      .descriptorAddress = *descriptorAddress,
      .nextCommandAddress = *nextCommandAddress,
      .word = word,
      .primitiveCount = primitiveCount,
      .opcode = opcode,
      .vertexCount = vertexCount,
      .descriptorStride = descriptorStride,
      .materialTableOffset = static_cast<uint16_t>((encodedWord >> 4u) & 0x01F0u),
      .blendVariant = static_cast<uint8_t>((encodedWord >> 5u) & 3u),
      .updatesMaterialState = word >= 0,
      .terminal = terminal,
  };
}

std::optional<ResidentMeshPrimitive>
decodeResidentMeshPrimitive(Core &core, const ResidentMeshCommand &command, uint32_t primitiveIndex) {
  if (command.terminal || primitiveIndex >= static_cast<uint16_t>(command.primitiveCount)) {
    return std::nullopt;
  }
  const std::optional<uint32_t> address =
      checkedResidentAddress(command.descriptorAddress,
                             static_cast<uint64_t>(primitiveIndex) * command.descriptorStride,
                             command.descriptorStride);
  if (!address) {
    return std::nullopt;
  }
  const uint32_t packedIndices = core.mem_r32(*address);
  ResidentMeshPrimitive primitive{
      .address = *address,
      .vertexIndices =
          {
              static_cast<uint8_t>(packedIndices),
              static_cast<uint8_t>(packedIndices >> 8u),
              static_cast<uint8_t>(packedIndices >> 16u),
              static_cast<uint8_t>(packedIndices >> 24u),
          },
  };
  if (command.descriptorStride == 12u) {
    primitive.attributeWords[0] = core.mem_r32(*address + 4u);
    primitive.attributeWords[1] = core.mem_r32(*address + 8u);
    if (command.vertexCount == 4u) {
      primitive.textureCoordinateWords = {
          static_cast<uint16_t>(primitive.attributeWords[0]),
          static_cast<uint16_t>(primitive.attributeWords[0] >> 16u),
          static_cast<uint16_t>(primitive.attributeWords[1]),
          static_cast<uint16_t>(primitive.attributeWords[1] >> 16u),
      };
      primitive.textureCoordinateCount = 4u;
    } else {
      primitive.textureCoordinateWords = {
          static_cast<uint16_t>(primitive.attributeWords[0] >> 16u),
          static_cast<uint16_t>(primitive.attributeWords[1]),
          static_cast<uint16_t>(primitive.attributeWords[1] >> 16u),
          0u,
      };
      primitive.textureCoordinateCount = 3u;
    }
  }
  return primitive;
}

std::optional<ResidentMeshCommandSummary> summarizeResidentMeshCommands(Core &core, const ResidentMeshLayout &layout) {
  const uint32_t firstPhysicalAddress = layout.commandAddress & 0x1FFFFFFFu;
  if (!guestRamRange(layout.commandAddress, sizeof(uint32_t))) {
    return std::nullopt;
  }
  const uint32_t maximumCommandCount = (kGuestRamSize - firstPhysicalAddress) / sizeof(uint32_t);
  uint32_t commandAddress = layout.commandAddress;
  ResidentMeshCommandSummary summary;
  for (uint32_t index = 0; index < maximumCommandCount; ++index) {
    const std::optional<ResidentMeshCommand> command = decodeResidentMeshCommand(core, commandAddress);
    if (!command) {
      return std::nullopt;
    }
    ++summary.commandCount;
    if (command->terminal) {
      summary.terminalOpcode = command->opcode;
      return summary;
    }

    summary.primitiveOpcodeMask |= uint32_t{1} << command->opcode;
    summary.primitiveCount += static_cast<uint16_t>(command->primitiveCount);
    if (command->updatesMaterialState) {
      summary.materialTableSlotMask |= uint32_t{1} << (command->materialTableOffset >> 4u);
      summary.blendVariantMask |= static_cast<uint8_t>(uint8_t{1} << command->blendVariant);
    }
    if ((command->nextCommandAddress & 0x1FFFFFFFu) <= (commandAddress & 0x1FFFFFFFu)) {
      return std::nullopt;
    }
    commandAddress = command->nextCommandAddress;
  }
  return std::nullopt;
}

ResidentMeshMaterialCensus censusResidentMeshMaterials(Core &core,
                                                       const ResidentMeshLayout &layout,
                                                       std::span<ResidentMeshDescriptorSample> samples) {
  ResidentMeshMaterialCensus census;
  if (!guestRamRange(layout.commandAddress, sizeof(uint32_t))) {
    return census;
  }

  ResidentMeshMaterialState material = readMaterialState(core, kResidentInitialMaterialTableOffset, 0u);
  const uint32_t firstPhysicalAddress = layout.commandAddress & 0x1FFFFFFFu;
  const uint32_t maximumCommandCount = (kGuestRamSize - firstPhysicalAddress) / sizeof(uint32_t);
  uint32_t commandAddress = layout.commandAddress;
  for (uint32_t index = 0; index < maximumCommandCount; ++index) {
    const std::optional<ResidentMeshCommand> command = decodeResidentMeshCommand(core, commandAddress);
    if (!command) {
      return census;
    }
    ++census.commandCount;
    if (command->updatesMaterialState) {
      material = readMaterialState(
          core, command->materialTableOffset, static_cast<uint16_t>(static_cast<uint16_t>(command->word) & 0x0060u));
      ++census.materialStateUpdateCount;
    }
    if (command->terminal) {
      census.complete = true;
      return census;
    }
    for (uint32_t primitiveIndex = 0; primitiveIndex < static_cast<uint16_t>(command->primitiveCount);
         ++primitiveIndex) {
      const std::optional<ResidentMeshPrimitive> primitive =
          decodeResidentMeshPrimitive(core, *command, primitiveIndex);
      if (!primitive || census.descriptorCount == std::numeric_limits<uint32_t>::max()) {
        return census;
      }
      ++census.descriptorCount;
      if (census.sampleCount == samples.size()) {
        census.descriptorSampleOverflow = true;
        continue;
      }
      samples[census.sampleCount++] = makeDescriptorSample(core, *command, primitiveIndex, *primitive, material);
    }
    if ((command->nextCommandAddress & 0x1FFFFFFFu) <= (commandAddress & 0x1FFFFFFFu)) {
      return census;
    }
    commandAddress = command->nextCommandAddress;
  }
  return census;
}

} // namespace ts2
