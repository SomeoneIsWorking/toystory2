#pragma once

#include "emit_memory.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace ts2 {

// Source layout consumed by resident mesh submitter 0x800100E4. A positive header places the command
// stream after N eight-byte vertices; a non-positive one adds a four-byte header and N auxiliary records.
struct ResidentMeshLayout {
  uint32_t meshAddress = 0;
  uint32_t vertexAddress = 0;
  uint32_t commandAddress = 0;
  int32_t headerWord = 0;
  uint32_t vertexCount = 0;
  bool hasAuxiliaryVertexRecords = false;
};

// A halfword whose low five bits select the primitive kind, a halfword primitive count, then that many
// descriptors (12 bytes below opcode 16, 4 bytes from it). Opcodes 24 and up end the stream.
struct ResidentMeshCommand {
  uint32_t address = 0;
  uint32_t nextCommandAddress = 0;
  uint16_t primitiveCount = 0;
  uint8_t opcode = 0;
  bool terminal = false;
};

std::optional<ResidentMeshLayout> decodeResidentMeshLayout(const psx::present::EmitMemory &memory,
                                                           uint32_t meshAddress);
std::optional<ResidentMeshCommand> decodeResidentMeshCommand(const psx::present::EmitMemory &memory,
                                                             uint32_t commandAddress);

// A whole mesh: its layout, every command through the terminal, and the address after the terminal's header.
struct ResidentMeshStream {
  ResidentMeshLayout layout;
  std::vector<ResidentMeshCommand> commands;
  uint32_t primitives = 0;
  uint32_t end = 0;
};

std::optional<ResidentMeshStream> walkResidentMesh(const psx::present::EmitMemory &memory, uint32_t meshAddress);

} // namespace ts2
