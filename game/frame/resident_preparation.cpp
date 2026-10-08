#include "frame/resident_preparation.h"

#include "core.h"
#include "runtime/toystory2_context.h"

#include <array>

namespace ts2 {
namespace {

// Level-start routine 0x8007BEC4, called from the main loop at 0x8007AE14.
constexpr uint32_t kLevelStart = 0x8007BEC4u;
constexpr uint32_t kLevelStartReturn = 0x8007AE18u;
// Main-loop block at 0x8007AE20, reached when 0x8007BEC4 returned zero: it stores the play loop's
// entry state and branches into the loop head 0x8007AEAC, whose first instruction is the field barrier 0x8003FA68.
constexpr uint32_t kPlayLoopEntryState = 0x8007AE20u;
// Return sentinel for finite host-initiated guest calls.
constexpr uint32_t kMainLoopEntry = 0x8007A9E8u;
// Arguments 0x8007BEC4 reads from guest RAM instead of $a0/$a1.
constexpr uint32_t kLevelId = 0x800A16A8u;
constexpr uint32_t kPlaybackMode = 0x800A120Cu;

} // namespace

ResidentPreparationProgress ResidentPreparation::step(Core &core, uint32_t level, int playbackMode) {
  if (!levelStart_.has_value()) {
    levelStart_.emplace();
    core.mem_w32(kLevelId, level);
    core.mem_w32(kPlaybackMode, static_cast<uint32_t>(playbackMode));
    // Attract routes only: FUN_8007C278's first presentation shows the LOADING card, which the instant CD makes
    // pointless.
    context(core).levelStartPresentation.arm(core, playbackMode);
    const std::array arguments{level};
    levelStart_->begin(core, {kLevelStart, kLevelStartReturn, arguments, std::nullopt, "resident level start"});
    context(core).yieldAtFieldBarrier = true;
  }
  if (levelStart_->pending()) {
    if (levelStart_->advance(core) != FieldCall::Step::returned) {
      // Still inside the routine: a display field or a host slice.
      return ResidentPreparationProgress::pending;
    }
    context(core).yieldAtFieldBarrier = false;
    // Nonzero: transition cut short by the boot countdown; the main loop skips the entry state (0x8007B230).
    if (levelStart_->result(core) != 0) {
      return ResidentPreparationProgress::finished;
    }
    levelStart_ = std::nullopt;
  }
  if (!playLoopEntry_.has_value()) {
    playLoopEntry_.emplace();
    playLoopEntry_->begin(core, {kPlayLoopEntryState, kMainLoopEntry, {}, std::nullopt, "resident play-loop entry"});
    context(core).yieldAtFieldBarrier = true;
  }
  const FieldCall::Step progress = playLoopEntry_->advance(core);
  context(core).yieldAtFieldBarrier = false;
  // The guest stays parked in its field barrier; the host owns the loop body (0x8007B254/0x8007B850) from here.
  playLoopEntry_->giveUp();
  playLoopEntry_ = std::nullopt;
  if (progress != FieldCall::Step::fieldBoundary) {
    // Unreachable: the entry stores satisfy the loop condition `DAT_800a136e == 0 || DAT_800a155c != 0`.
    lucent::error("ts2-execution", "resident play-loop entry left the guest loop without waiting at its field barrier");
    std::abort();
  }
  return ResidentPreparationProgress::ready;
}

} // namespace ts2
