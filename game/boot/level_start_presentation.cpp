#include "boot/level_start_presentation.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "native_dispatch.h"
#include "runtime/toystory2_context.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

// The two words FUN_8007C278 clears on entry, before its loop and independent of it.
constexpr std::uint32_t kPresentationCounterA = 0x800A0CE0u;
constexpr std::uint32_t kPresentationCounterB = 0x800A0CE4u;
// The attract/demo flag, read by FUN_8007C278's own guard as its second argument.
constexpr std::uint32_t kPlaybackMode = 0x800A120Cu;
// DAT_800A120C values that do NOT trip the guard: the level's own graphic shows on these routes.
constexpr int kDemoGuardExemptMode = 0x7B;

} // namespace

bool LevelStartPresentation::demoGuardForcesLoadingCard(int demoMode) {
  return demoMode != 0 && demoMode != kDemoGuardExemptMode;
}

void LevelStartPresentation::arm(Core &core, int playbackMode) {
  const auto image = core.currentImageIdentity(levelStartPresentationAddress());
  if (!image) {
    lucent::error("ts2-level-start",
                  "refused to arm the level start's first presentation: no resident image identity at 0x{:08X}",
                  levelStartPresentationAddress());
    std::abort();
  }
  const psx::cpu::NativeKey key{*image, levelStartPresentationAddress()};
  const bool wanted = demoGuardForcesLoadingCard(playbackMode);
  if (wanted == core.nativeDispatcher().isInstalled(key)) {
    return;
  }
  if (wanted) {
    installResidentOverride(
        core, levelStartPresentationAddress(), "level-start-first-presentation", levelStartFirstPresentationEntry);
    return;
  }
  if (!core.nativeDispatcher().remove(key)) {
    lucent::error("ts2-level-start", "failed to retire the level start's first-presentation override");
    std::abort();
  }
}

void levelStartFirstPresentationEntry(Core *core) {
  context(*core).levelStartPresentation.run(*core);
}

void LevelStartPresentation::run(Core &core) {
  const int demoMode = static_cast<int>(core.mem_r32(kPlaybackMode));
  if (!demoGuardForcesLoadingCard(demoMode)) {
    // The override is armed only for a route whose own guard rewrites the requested graphic to the
    // game's LOADING card, so this is unreachable: reaching it means the arming decision and the
    // guest's own guard disagree, and running the card would be worse than saying so.
    lucent::error("ts2-level-start", "the level start's first-presentation override fired with demo mode {}", demoMode);
    std::abort();
  }
  // The card is not presented and not waited for: under instant CD the load that would have covered
  // it has already completed, so presenting it only delays the level's first real picture. The
  // routine's own unconditional entry stores are still made, so the state the level start expects is
  // the state it would have found, and its fade is left to FUN_8007C344, which re-establishes one
  // with FUN_80077598(0,0,0,0xc) before it draws anything. Driving the fade here would be a timer
  // write pretending to be progress.
  core.mem_w32(kPresentationCounterA, 0);
  core.mem_w32(kPresentationCounterB, 0);
  core.r[2] = 0; // void routine; the guest leaves $v0 undefined and no caller reads it
}

} // namespace ts2
