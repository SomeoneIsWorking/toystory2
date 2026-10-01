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
// The two arguments 0x8007BEC4 reads from guest RAM rather than from `$a0`/`$a1`, at the two places
// its own caller stores them.
constexpr uint32_t kLevelId = 0x800A16A8u;
constexpr uint32_t kPlaybackMode = 0x800A120Cu;

} // namespace

ResidentPreparationProgress ResidentPreparation::step(Core &core, uint32_t level, int playbackMode) {
  if (!levelStart_.has_value()) {
    levelStart_.emplace(core);
  }
  if (!levelStart_->active()) {
    core.mem_w32(kLevelId, level);
    core.mem_w32(kPlaybackMode, static_cast<uint32_t>(playbackMode));
    const std::array arguments{level};
    levelStart_->begin({kLevelStart, kLevelStartReturn, arguments, std::nullopt, "resident level start"});
    context(core).yieldAtFieldBarrier = true;
  }
  if (levelStart_->advance() == ResumableGuestCall::Progress::fieldBoundary) {
    // Still inside the routine's own transition loop: `[0x800A1174]` holds the fields the wait
    // covered, `[0x800A1480]` and `[0x800A155C]` are the guest's own, and the next field is presented.
    return ResidentPreparationProgress::pending;
  }
  context(core).yieldAtFieldBarrier = false;
  // 0x8007BEC4 returns nonzero when its transition was cut short by the boot countdown, which is the
  // case the outer loop treats as an interrupted level start rather than a ready one.
  return levelStart_->result() != 0 ? ResidentPreparationProgress::finished : ResidentPreparationProgress::ready;
}

} // namespace ts2
