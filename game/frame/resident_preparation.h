#pragma once

#include "frame/outer_loop.h"

#include "frame/field_call.h"

#include <cstdint>
#include <cstdlib>
#include <optional>

class Core;

namespace ts2 {

// Runs the guest's level start 0x8007BEC4(level) then the main loop's entry block at 0x8007AE20, one
// presented display field per step. The entry block parks the guest in the play loop's field barrier.
class ResidentPreparation {
public:
  ResidentPreparationProgress step(Core &core, uint32_t level, int playbackMode);

private:
  std::optional<FieldCall> levelStart_{};
  std::optional<FieldCall> playLoopEntry_{};
};

} // namespace ts2
