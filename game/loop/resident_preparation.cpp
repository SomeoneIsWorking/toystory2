#include "loop/resident_preparation.h"

#include "core.h"
#include "toystory2_context.h"

#include <array>

namespace ts2 {
namespace {

// Retail 0x8007BEC4, the level-start routine, and its call site at 0x8007AE14 in the main loop:
// `lw $4, DAT_800a16a8; sw $4, DAT_800a16a8; jal 0x8007bec4`. Its return address is the
// instruction after that call.
constexpr uint32_t kLevelStart = 0x8007BEC4u;
constexpr uint32_t kLevelStartReturn = 0x8007AE18u;
// The block the main loop reaches at 0x8007AE20 once 0x8007BEC4 returned zero (`bnez $v0, 0x8007B230`
// is the other leg). DECOMPILED WHOLE from the exact bytes: eleven stores that publish the play
// loop's entry state -- the transition flags at [0x800A1480], [0x800A11E4], [0x800A10F4],
// [0x800A11A4], [0x800A15BC], [0x800A1324], the two halfwords at [0x800A1638], the elapsed-field
// word [0x800A1174], the loop exit reason [0x800A136E], the exit countdown [0x800A155C] = 0x5A --
// then `[0x800A1430] = [0x800C166C] << 1`, the fade at 0x80077598 and the branch into the loop head
// 0x8007AEAC, whose first instruction is the guest's own two-field barrier 0x8003FA68. Running this
// block therefore establishes the entry state and stops on the field barrier that starts the play
// loop, with every word written by the stores that own it.
constexpr uint32_t kPlayLoopEntryState = 0x8007AE20u;
// Main 0x8007A9E8 is the return sentinel every finite host-initiated guest call in this port uses. The
// entry block cannot reach it: it either blocks in the loop's own field barrier or falls out of the
// loop into the resident exit, which returns through this same address.
constexpr uint32_t kMainLoopEntry = 0x8007A9E8u;
// The two arguments 0x8007BEC4 reads from guest RAM rather than from `$a0`/`$a1`, at the two places
// its own caller stores them.
constexpr uint32_t kLevelId = 0x800A16A8u;
constexpr uint32_t kPlaybackMode = 0x800A120Cu;

} // namespace

ResidentPreparationProgress ResidentPreparation::step(Core &core, uint32_t level, int playbackMode) {
  if (!levelStart_.has_value()) {
    levelStart_.emplace(core);
    core.mem_w32(kLevelId, level);
    core.mem_w32(kPlaybackMode, static_cast<uint32_t>(playbackMode));
    // The level start's FIRST presentation (FUN_8007C278) presents the game's LOADING card on the
    // attract routes only, where the instant-CD port has nothing left for it to cover. That
    // suppression is armed here, from the same `DAT_800A120C` the guest's own guard reads, and only
    // there: on a player level the routine is not intercepted at all and the level's own card is
    // presented by the level start's own field-spanning call.
    context(core).levelStartPresentation.arm(core, playbackMode);
    const std::array arguments{level};
    levelStart_->begin({kLevelStart, kLevelStartReturn, arguments, std::nullopt, "resident level start"});
    context(core).yieldAtFieldBarrier = true;
  }
  if (levelStart_->active()) {
    if (levelStart_->advance() != ResumableGuestCall::Progress::returned) {
      // Still inside the routine: a display field at its own transition barrier, or a host slice
      // (the level start's first presentation hands the turn back between its own compute slices).
      // Either way `[0x800A1174]` holds the fields the wait covered, `[0x800A1480]` and
      // `[0x800A155C]` are the guest's own, and the next field is presented.
      return ResidentPreparationProgress::pending;
    }
    context(core).yieldAtFieldBarrier = false;
    // 0x8007BEC4 returns nonzero when its transition was cut short by the boot countdown, which is the
    // case the outer loop treats as an interrupted level start rather than a ready one. The main loop
    // takes its `bnez` leg to 0x8007B230 in that case, so the entry state below is not run.
    if (levelStart_->result() != 0) {
      return ResidentPreparationProgress::finished;
    }
    levelStart_ = std::nullopt;
  }
  if (!playLoopEntry_.has_value()) {
    playLoopEntry_.emplace(core);
    playLoopEntry_->begin({kPlayLoopEntryState, kMainLoopEntry, {}, std::nullopt, "resident play-loop entry"});
    context(core).yieldAtFieldBarrier = true;
  }
  const ResumableGuestCall::Progress progress = playLoopEntry_->advance();
  context(core).yieldAtFieldBarrier = false;
  // The guest is left parked inside its own field barrier, which is exactly where it is at every
  // display field; the host owns the loop body (0x8007B254/0x8007B850) from the next field, as it
  // already does for every field after this one, so this call is never resumed.
  playLoopEntry_->abandon();
  playLoopEntry_ = std::nullopt;
  if (progress != ResumableGuestCall::Progress::fieldBoundary) {
    // Unreachable: the eleven stores above publish `[0x800A136E] = 0` and `[0x800A155C] = 0x5A`, which
    // is the loop condition `DAT_800a136e == 0 || DAT_800a155c != 0` the entry block branches on, so
    // the block always reaches 0x8007AEAC's barrier instead of falling out of the loop.
    lucent::error("ts2-execution", "resident play-loop entry left the guest loop without waiting at its field barrier");
    std::abort();
  }
  return ResidentPreparationProgress::ready;
}

} // namespace ts2
