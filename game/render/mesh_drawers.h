// The ported resident mesh drawers behind one entry: the guest override and the render both run a call
// through here.
#pragma once

#include "emit_memory.h"
#include "render/mesh_draw_call.h"

#include <optional>

namespace ts2 {

class MeshDrawers {
public:
  // The call's inputs when its drawer has a native body for all of it; nullopt leaves it on the guest original.
  static std::optional<MeshDrawInputs> plan(const psx::present::EmitMemory &memory, const MeshDrawCall &call);

  // nullopt when the call reaches a path without a native body: the memory is then dirty and must be discarded.
  static std::optional<MeshDrawResult> run(const psx::present::EmitMemory &memory, const MeshDrawCall &call);

  // The ordering table the call links into.
  static std::uint32_t orderingTable(const psx::present::EmitMemory &memory, const MeshDrawCall &call);
};

} // namespace ts2
