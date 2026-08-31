#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

class Core;

namespace ts2 {

// The resident submitter starts from this entry, then replaces it with
// `(uint16_t(word) >> 4) & 0x1F0` whenever a non-negative command changes material state. These
// are byte offsets, not abstract material ids: applying the mask before the shift selects the
// command's blend bits instead of its material-table slot.
inline constexpr uint32_t kResidentMaterialTableBase = 0x800CD200u;
inline constexpr uint16_t kResidentInitialMaterialTableOffset = 0x00E0u;
inline constexpr size_t kMaxResidentMeshDescriptorSamples = 32;

// Source layout consumed by resident mesh submitter 0x800100E4 before any GTE operation or packet
// write. Positive headers place the command stream after N eight-byte vertices. Non-positive headers
// add one four-byte header and N four-byte auxiliary records after the same vertex array.
struct ResidentMeshLayout {
  uint32_t meshAddress = 0;
  uint32_t vertexAddress = 0;
  uint32_t commandAddress = 0;
  int32_t headerWord = 0;
  uint32_t vertexCount = 0;
  bool hasAuxiliaryVertexRecords = false;
};

struct ResidentMeshVertex {
  uint32_t address = 0;
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
  // 0x800100E4 expands bits 10..14, 5..9 and 0..4 into the red, green and blue command bytes.
  uint16_t color555 = 0;
};

struct ResidentMeshCommand {
  uint32_t address = 0;
  uint32_t descriptorAddress = 0;
  uint32_t nextCommandAddress = 0;
  int16_t word = 0;
  int16_t primitiveCount = 0;
  uint8_t opcode = 0;
  uint8_t vertexCount = 0;
  uint8_t descriptorStride = 0;
  // `((uint16_t(word) >> 4) & 0x1F0)` is a byte offset into kResidentMaterialTableBase.
  uint16_t materialTableOffset = 0;
  uint8_t blendVariant = 0;
  bool updatesMaterialState = false;
  bool terminal = false;
};

struct ResidentMeshPrimitive {
  uint32_t address = 0;
  std::array<uint8_t, 4> vertexIndices{};
  std::array<uint32_t, 2> attributeWords{};
  // Textured 12-byte groups consume four packed UV halfwords for quads. Triangle groups skip the
  // low halfword of the first attribute word and consume the remaining three in draw order.
  std::array<uint16_t, 4> textureCoordinateWords{};
  uint8_t textureCoordinateCount = 0;
};

struct ResidentMeshCommandSummary {
  uint32_t primitiveOpcodeMask = 0;
  uint32_t materialTableSlotMask = 0;
  uint8_t blendVariantMask = 0;
  uint8_t terminalOpcode = 0;
  uint32_t commandCount = 0;
  uint64_t primitiveCount = 0;
};

// Raw table data consumed by 0x800100E4 before it expands the selected material state into a
// primitive. The field names deliberately preserve the observed load offsets until their semantics
// are independently established.
struct ResidentMeshMaterialState {
  uint16_t tableOffset = 0;
  uint16_t blendBits = 0;
  uint32_t tableAddress = 0;
  uint32_t tableWord0 = 0;
  uint16_t tableWord8 = 0;
};

struct ResidentMeshDescriptorSample {
  uint32_t commandAddress = 0;
  uint32_t descriptorAddress = 0;
  uint16_t commandWord = 0;
  uint16_t primitiveIndex = 0;
  ResidentMeshMaterialState material{};
  std::array<uint32_t, 3> descriptorWords{};
};

// `descriptorCount` is the complete source denominator. `sampleCount` is only the retained prefix;
// a true overflow says that descriptor samples are incomplete rather than quietly implying a short
// mesh stream.
struct ResidentMeshMaterialCensus {
  uint32_t commandCount = 0;
  uint32_t materialStateUpdateCount = 0;
  uint32_t descriptorCount = 0;
  uint32_t sampleCount = 0;
  bool descriptorSampleOverflow = false;
  bool complete = false;
};

std::optional<ResidentMeshLayout> decodeResidentMeshLayout(Core &core, uint32_t meshAddress);
std::optional<ResidentMeshVertex>
decodeResidentMeshVertex(Core &core, const ResidentMeshLayout &layout, uint32_t vertexIndex);
std::optional<ResidentMeshCommand> decodeResidentMeshCommand(Core &core, uint32_t commandAddress);
std::optional<ResidentMeshPrimitive>
decodeResidentMeshPrimitive(Core &core, const ResidentMeshCommand &command, uint32_t primitiveIndex);
std::optional<ResidentMeshCommandSummary> summarizeResidentMeshCommands(Core &core, const ResidentMeshLayout &layout);
ResidentMeshMaterialCensus censusResidentMeshMaterials(Core &core,
                                                       const ResidentMeshLayout &layout,
                                                       std::span<ResidentMeshDescriptorSample> samples);

} // namespace ts2
