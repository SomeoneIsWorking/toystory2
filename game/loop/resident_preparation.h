#pragma once

#include "loop/outer_loop.h"

#include "core/guest_execution.h"

#include <cstdint>
#include <cstdlib>
#include <optional>

class Core;

namespace ts2 {

// The resident level start is the GUEST'S routine: retail 0x8007BEC4(level), reached from the main
// loop at 0x8007AE14 with the selected level in `$a0` and the level id it stores at `[0x800A16A8]`.
// It loads the level overlay (0x8007C278), decodes its assets (0x8003D88C), runs the fade transition
// (0x8007C344), re-initialises the drawing environment (0x80039D9C) and then writes the ~40 words the
// resident scene starts from, including the exit countdown at `[0x800A155C]`, the timer at
// `[0x800A1370]` and the per-object table resets. This owner used to replay those words and that loop
// by hand; it now runs the routine itself, so every one of them is written by the code that owns it.
//
// The one wait inside it, the transition's field barrier at 0x8003FA68, is already natively owned: it
// publishes the number of fields the wait covered at `[0x800A1174]`, services the deferred display,
// and yields to the host so each authored field is presented. The level start therefore spans display
// fields exactly like the front-end poll and the resident update, one presented field per step.
//
// A successful level start is followed by the main loop's own entry state at 0x8007AE20, which this
// owner also runs rather than replaying. The guest ends that block parked in the play loop's own
// two-field barrier, and the frame driver owns the loop body from the next field exactly as it owns
// every later resident update.
class ResidentPreparation {
public:
  ResidentPreparationProgress step(Core &core, uint32_t level, int playbackMode);

private:
  // Constructed with the Core on the first step: a resumable call is bound to one executor.
  std::optional<ResumableGuestCall> levelStart_{};
  std::optional<ResumableGuestCall> playLoopEntry_{};
};

} // namespace ts2
