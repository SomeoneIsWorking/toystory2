// Toy Story 2's resident VBlank delivery seam.
//
// Ghidra and exact generated instructions establish the complete chain:
//
//   0x8003A650 graphics init
//     -> 0x800888BC(callback=0x80039D60)
//     -> 0x80039D60 VBlank handler
//        -> 0x80021028 deferred display work
//        -> increment the field accumulator at gp+0x7FC
//
// The guest's 0x8003FA68 frame wait consumes that accumulator, requests the
// deferred display work, and waits for the same handler to acknowledge it. A
// static recompile otherwise has no host turn while that guest loop runs. The
// host owns when a display field occurs; the intact guest handler continues to
// own everything the field does.
#include "field_clock.h"

#ifdef TS2_HAVE_SUBSTRATE

#include "core.h"
#include "game.h"
#include "rec_decls.h"
#include "recomp_iface.h"
#include <lucent/log.h>

namespace {

constexpr uint32_t kGraphicsInit = 0x8003A650u;
constexpr uint32_t kGuestVBlankHandler = 0x80039D60u;

bool s_clockArmed = false;

void field_turn(Core *core) {
  // Input is sampled at the field boundary before guest work observes it.
  core->game->pad.serviceFrame();

  // Dispatch the callback the executable itself registered. It increments the
  // field accumulator and performs deferred buffer work when requested.
  rec_dispatch(core, kGuestVBlankHandler);

  // This port still runs the guest-owned frame loop, so its field boundary is
  // also responsible for advancing the audio sink and presenting guest VRAM.
  core->game->spu_audio.frame();
  gpu_present(core);
}

void graphics_init(Core *core) {
  // The guest establishes GPU state and registers kGuestVBlankHandler first.
  // Arming earlier could dispatch it against uninitialized graphics state.
  gen_func_8003A650(core);
  if (s_clockArmed) {
    return;
  }

  rec_host_turn_register(core, field_turn, gpu_field_rate_millihz(core));
  s_clockArmed = true;
  lucent::info("ts2-field", "field delivery armed after guest graphics init registered 0x{:08X}", kGuestVBlankHandler);
}

} // namespace

#endif

void ts2_field_clock_install() {
#ifdef TS2_HAVE_SUBSTRATE
  psxport_recomp()->shard_set_override(kGraphicsInit, graphics_init);
#endif
}
