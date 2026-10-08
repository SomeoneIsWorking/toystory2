#include "fmv/movie_player.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "game.h"
#include "native_dispatch.h"
#include "native_fmv.h"

#include <lucent/log.h>

#include <cstdint>
#include <string>

namespace ts2::fmv {
namespace {

// FUN_800D7088, the FMV module's movie player, called from FUN_800D6628.
constexpr std::uint32_t kMoviePlayer = 0x800D7088u;

// The cold front end sets this once its first frame is up; the retail player returns it as the skip result.
constexpr std::uint32_t kColdStartFlag = 0x800A1670u;
constexpr std::size_t kMaxGuestPath = 128u;

void moviePlayerOverride(Core *core) {
  const std::string path = guestString(*core, core->r[4], kMaxGuestPath);
  if (path.empty()) {
    lucent::error("ts2-fmv", "movie player refused: empty path at 0x{:08X}", core->r[4]);
    core->r[2] = 0;
    return;
  }
  if (core->game->fmv.finished()) {
    if (!core->game->fmv.beginPath(path.c_str())) {
      core->r[2] = 0; // could not be opened; answer as a movie that never played
      return;
    }
  } else {
    core->game->fmv.step();
  }
  if (!core->game->fmv.finished()) {
    // One movie frame per host turn, so the frame loop, control channel and pad events run between frames.
    psx::cpu::requestExecutionExit(
        *core,
        psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::CooperativeYield, kMoviePlayer, 0, "movie frame"});
    return;
  }
  // Zero at end of movie, the cold-start word when skipped.
  core->r[2] = core->game->fmv.skipped() ? core->mem_r32(kColdStartFlag) : 0u;
}

} // namespace

void GuestMoviePlayer::install(Core &core) {
  psx::cpu::installNativeOverride(core, kMoviePlayer, "guest-movie-player", moviePlayerOverride);
}

} // namespace ts2::fmv
