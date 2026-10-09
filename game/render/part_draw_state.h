// A part drawer call as a producer state, and its render.
//
// The state is what the drawer reads: its registers, the GTE control registers (rotation and translation the
// renderer composed for the part, light and background colours), and the guest memory `PartFaceDrawers::inputs`
// names (part record, face list, normals, texture page entry, light vector, globals). The render runs the
// drawer's own body again over host copies of those bytes with the control registers `t` of the way between
// two states, and reads the packets it linked out of a host copy of the ordering table.
#pragma once

#include "render/part_face_drawers.h"
#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace ts2 {

// Reads a drawer call's inputs before the body runs and saves them under the object scope open now, once the
// body has linked a packet. Nothing is saved when the drawer's table is not a named ordering table.
class PartDrawRecorder {
public:
  PartDrawRecorder(Core &core, const PartDrawCall &call);

  void save(Core &core) const;

private:
  PartDrawCall call_;
  std::vector<std::byte> state_;
  std::uint32_t cursor_ = 0;
  bool named_ = false;
};

class PartStateRender final : public psx::present::StateProducer {
public:
  explicit PartStateRender(Core &core) : core_(core) {}

  void render(std::span<const std::byte> from,
              std::span<const std::byte> to,
              float t,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

} // namespace ts2
