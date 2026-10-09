// The static mesh submitter 0x800100E4, ported: a command stream of textured and plain quads and triangles over
// an eight-byte vertex array, each primitive projected, culled, given a retained packet slot, subdivided when it
// is near the camera, and linked into the ordering table. One body serves the guest override, the trial that
// vets a call and the render of a saved call.
#pragma once

#include "emit_memory.h"
#include "render/mesh_draw_call.h"

#include <optional>

namespace ts2 {

class StaticMeshDrawer {
public:
  inline static constexpr std::uint32_t kEntry = 0x800100E4u;

  // Whether a command with this opcode has a native handler.
  static bool handles(std::uint32_t opcode);

  // The call's inputs when its mesh is one the body handles and a trial run over host copies of them reaches
  // only native code; nullopt leaves the call on the guest original.
  static std::optional<MeshDrawInputs> plan(const psx::present::EmitMemory &memory, const MeshDrawCall &call);

  // nullopt when the call reaches a path with no native body; memory is then dirty and must be discarded.
  static std::optional<MeshDrawResult> run(const psx::present::EmitMemory &memory, const MeshDrawCall &call);

  // The ordering table the call links into.
  static std::uint32_t orderingTable(const psx::present::EmitMemory &memory);
};

} // namespace ts2
