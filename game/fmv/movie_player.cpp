#include "fmv/movie_player.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "execution_control.h"
#include "execution_exit.h"
#include "game.h"
#include "native_fmv.h"

#include <lucent/log.h>

#include <cstdint>
#include <string>

namespace ts2::fmv {
namespace {

// FUN_800D7088 — the FMV module's movie player. `jal 0x800d7088` is the single call site, from
// FUN_800D6628, the module entry the front-end sequencer enters by movie index.
constexpr std::uint32_t kMoviePlayer = 0x800D7088u;

// `_DAT_800A1670` in the FMV module's decompilation: the guest word the cold front end sets once its
// first frame is up (game/frame/frame_driver.cpp writes it in finishColdFrontEnd). The retail
// player returns it as the skip result, and zeroes its `arg` when it is already set, so during the
// cold intro a skip returns 1 and ends the intro sequence.
constexpr std::uint32_t kColdStartFlag = 0x800A1670u;

std::string guestPath(Core &core, std::uint32_t pointer) {
  std::string text;
  for (std::uint32_t offset = 0; offset < 128u; ++offset) {
    const auto c = static_cast<char>(core.mem_r8(pointer + offset));
    if (c == 0) {
      return text;
    }
    text.push_back(c);
  }
  return text;
}

void moviePlayerOverride(Core *core) {
  // The guest's own argument names the movie; nothing here re-decides which movie to play.
  const std::string path = guestPath(*core, core->r[4]);
  if (path.empty()) {
    lucent::error("ts2-fmv", "movie player refused: empty path at 0x{:08X}", core->r[4]);
    core->r[2] = 0;
    return;
  }
  if (core->game->fmv.finished()) { // first entry into this call: open the movie
    if (!core->game->fmv.beginPath(path.c_str())) {
      core->r[2] = 0; // could not be opened; answer as a movie that never played
      return;
    }
  } else {
    core->game->fmv.step();
  }
  if (!core->game->fmv.finished()) {
    // ONE host turn, one movie frame, then hand the turn back. This is what makes the movie
    // interruptible at all: the frame loop, the control channel and the window's events all get a
    // slice between frames, so a Start press arrives through the ordinary pad path and `pshot` can
    // see a movie frame. Resuming at this same entry is the continuation — the guest call stays
    // suspended inside the retail player until the movie ends, exactly as it would inside the
    // guest's own streaming loop.
    psx::cpu::requestExecutionExit(
        *core,
        psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::CooperativeYield, kMoviePlayer, 0, "movie frame"});
    return;
  }
  // Answer exactly what the retail player answers: zero at end of movie, the cold-start word when
  // the guest skipped. The caller returns it to the front-end sequencer unchanged.
  core->r[2] = core->game->fmv.skipped() ? core->mem_r32(kColdStartFlag) : 0u;
}

} // namespace

void installGuestMoviePlayer(Core &core) {
  installResidentOverride(core, kMoviePlayer, "guest-movie-player", moviePlayerOverride);
}

} // namespace ts2::fmv
