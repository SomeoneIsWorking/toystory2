// The rigid mesh drawer 0x80017FF8, ported: a command stream of quads and triangles over a vertex array, each
// primitive projected, culled and linked into the ordering table through a retained packet slot. One body
// serves the guest override and the render of a saved call.
#pragma once

#include "emit_memory.h"
#include "render/mesh_draw_call.h"

namespace ts2 {

class RigidMeshDrawer {
public:
  inline static constexpr std::uint32_t kEntry = 0x80017FF8u;

  // Whether every command of the mesh has a ported body; a call with another stays on the guest original.
  static bool ported(const psx::present::EmitMemory &memory, const MeshDrawCall &call);

  static MeshDrawResult run(const psx::present::EmitMemory &memory, const MeshDrawCall &call);

  static MeshDrawInputs inputs(const psx::present::EmitMemory &memory, const MeshDrawCall &call);
};

} // namespace ts2
