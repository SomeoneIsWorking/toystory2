#include "input/recording_phase.h"

#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

constexpr std::uint32_t kPlaybackMode = 0x800A120Cu;    // the attract/demo flag; FUN_8007C278's own guard
constexpr std::uint32_t kColdFrontEnd = 0x800A1670u;    // set once the front end is up; the FMV player reads it
constexpr std::uint32_t kSelectionActive = 0x800A1420u; // the main menu's START GAME / CONTINUE GAME choice
constexpr std::uint32_t kLevelId = 0x800A16A8u;         // the selected level, the level start's own argument

} // namespace

std::uint64_t InputPhase::of(Core &core) const {
  const std::uint64_t phase = pack(
      core.mem_r32(kPlaybackMode), core.mem_r32(kColdFrontEnd), core.mem_r32(kSelectionActive), core.mem_r32(kLevelId));
  if (phase == psx::input::kUnkeyedPhase) {
    lucent::error("ts2-input", "the input phase packed to the reserved unkeyed value 0x{:016X}", phase);
    std::abort();
  }
  return phase;
}

} // namespace ts2
