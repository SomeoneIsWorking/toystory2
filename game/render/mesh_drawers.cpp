#include "render/mesh_drawers.h"

#include "render/rigid_mesh_drawer.h"
#include "render/static_mesh_drawer.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {

using psx::present::EmitMemory;

namespace {

[[noreturn]] void unknown(std::uint32_t drawer) {
  lucent::error("ts2-mesh", "0x{:08X} is not a ported mesh drawer", drawer);
  std::abort();
}

} // namespace

std::optional<MeshDrawInputs> MeshDrawers::plan(const EmitMemory &memory, const MeshDrawCall &call) {
  switch (call.drawer) {
  case RigidMeshDrawer::kEntry:
    return RigidMeshDrawer::ported(memory, call) ? std::optional(RigidMeshDrawer::inputs(memory, call)) : std::nullopt;
  case StaticMeshDrawer::kEntry:
    return StaticMeshDrawer::plan(memory, call);
  default:
    return std::nullopt;
  }
}

std::optional<MeshDrawResult> MeshDrawers::run(const EmitMemory &memory, const MeshDrawCall &call) {
  switch (call.drawer) {
  case RigidMeshDrawer::kEntry:
    return RigidMeshDrawer::run(memory, call);
  case StaticMeshDrawer::kEntry:
    return StaticMeshDrawer::run(memory, call);
  default:
    unknown(call.drawer);
  }
}

std::uint32_t MeshDrawers::orderingTable(const EmitMemory &memory, const MeshDrawCall &call) {
  switch (call.drawer) {
  case RigidMeshDrawer::kEntry:
    return call.args[2];
  case StaticMeshDrawer::kEntry:
    return StaticMeshDrawer::orderingTable(memory);
  default:
    unknown(call.drawer);
  }
}

} // namespace ts2
