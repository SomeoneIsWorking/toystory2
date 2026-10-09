// A resident mesh drawer call as a producer state, and its render.
//
// The state is what the drawer reads: its registers, the GTE control registers the caller composed for the
// mesh instance, and the guest memory `MeshDrawers::inputs` names (the mesh, its slot table, the free and
// released slot lists, the retained packets, the scratchpad the caller published, the globals). The render runs
// the drawer's own body again over host copies of those bytes with the control registers `t` of the way between
// two states, and reads the packets it linked out of a host copy of the ordering table.
#pragma once

#include "render/mesh_drawers.h"
#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace ts2 {

// Reads a call's inputs before the body runs and saves them under the object scope open now, once the body
// has linked a packet. Nothing is saved when the call's table is not a named ordering table.
class MeshDrawRecorder {
public:
  MeshDrawRecorder(Core &core, const MeshDrawCall &call, const MeshDrawInputs &inputs);

  void save(Core &core, const MeshDrawResult &result) const;

private:
  static void endLists(const MeshDrawInputs &inputs, std::vector<std::byte> &bytes);

  std::vector<std::byte> state_;
  bool named_ = false;
};

class MeshStateRender final : public psx::present::StateProducer {
public:
  explicit MeshStateRender(Core &core) : core_(core) {}

  void render(std::span<const std::byte> from,
              std::span<const std::byte> to,
              float t,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

} // namespace ts2
