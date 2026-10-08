#include "widescreen/resident_widescreen.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "execution_exit.h"
#include "game.h"
#include "native_dispatch.h"

#include <cstdint>

namespace ts2 {
namespace {

// Per-object visibility cull, called with the literal console frame (0, 0x200, 0, 0xf0) at 0x80029AAC and
// recursing with the parent's box. Its flag (DAT_800a8860) is read only by the mesh-submission passes.
constexpr std::uint32_t kObjectCullLeaf = 0x80027AF0u;

// The leaf clamps its stored box to column 0x200 (`slti $v0,$v0,0x200` at 0x800280D4) and boxes are
// conservative, so a canvas-sized window would drop room objects whose faces are on screen.
constexpr std::int32_t kCullWindowLeft = -32768;
constexpr std::int32_t kCullWindowRight = 32767;

// The mesh submitter publishes the box stored by the cull leaf, already clamped to column 512, and drops mesh
// records outside it; widening the cull rectangle alone does not change the frame.
constexpr std::uint32_t kScreenRectPublisherLeaf = 0x80010000u;
constexpr std::int32_t kScreenRectLeft = -4096;
constexpr std::int32_t kScreenRectRight = 4096;

void objectCullOverride(Core *core) {
  ResidentWidescreenCull::widenCullRect(*core);
  psx::cpu::callOriginalToReturn(
      *core, kObjectCullLeaf, psx::cpu::ExecutionBudget::currentTurn(*core), "object visibility cull original");
}

void screenRectPublisherOverride(Core *core) {
  ResidentWidescreenCull::widenScreenRect(*core);
  psx::cpu::callOriginalToReturn(
      *core, kScreenRectPublisherLeaf, psx::cpu::ExecutionBudget::currentTurn(*core), "screen rect publisher original");
}

} // namespace

void ResidentWidescreenCull::install(Core &core) {
  psx::cpu::installNativeOverride(core, kObjectCullLeaf, "object-visibility-cull", objectCullOverride);
  psx::cpu::installNativeOverride(core, kScreenRectPublisherLeaf, "screen-rect-publisher", screenRectPublisherOverride);
}

bool ResidentWidescreenCull::active(const Core &core) {
  return core.game != nullptr && core.game->guestDisplay.plan().widescreen();
}

void ResidentWidescreenCull::widenCullRect(Core &core) {
  if (!active(core)) {
    return;
  }
  // $a1/$a2 = left/right edge; the window only ever grows.
  core.r[5] = static_cast<std::uint32_t>(kCullWindowLeft);
  core.r[6] = static_cast<std::uint32_t>(kCullWindowRight);
}

void ResidentWidescreenCull::widenScreenRect(Core &core) {
  if (!active(core)) {
    return;
  }
  // $a0/$a1 = left/right edge of the published rectangle; the window only ever grows.
  core.r[4] = static_cast<std::uint32_t>(kScreenRectLeft);
  core.r[5] = static_cast<std::uint32_t>(kScreenRectRight);
}

} // namespace ts2
