// title_session.cpp — the boot sequence and the teardown, as one owner.
#include "boot/title_session.h"

#include "c_subsys.h" // watchdog_init, mdec_init (C-linkage subsystems)
#include "core.h"
#include "disc.h"
#include "fs_util.h"
#include "game.h"
#include "hw_bind.h" // gte_init
#include "psx_exe_image.h"

#include <lucent/log.h>

extern "C" {
// spu_init is the one boot symbol with no owning header: spu_beetle.cpp defines it with C linkage
// from the vendored Beetle SPU and declares nothing for the rest of the runtime.
void spu_init(void);
}

void native_boot_run(Core *c); // psxport native boot owner

namespace ts2 {

TitleSession::TitleSession(const char *exePath) : exePath_(exePath) {}

TitleSession::~TitleSession() = default;

bool TitleSession::selfProvision() {
  if (Fs::exists(exePath_)) {
    return true;
  }
  lucent::warn("boot", "{} missing — extracting from disc", exePath_);
  // Disc resolution: $PSXPORT_TS2_DISC, .env, or a *.chd in the working directory — the same order
  // tools/resolve_disc.py implements host-side.
  if (disc_extract_file(&game_->disc, kDiscExePath, exePath_)) {
    return true;
  }
  lucent::error("boot",
                "extraction failed: provide a disc (PSXPORT_TS2_DISC, .env, or "
                "a *.chd in the working directory), or run `uv run --frozen python "
                "tools/extract_exe.py`");
  return false;
}

void TitleSession::bringUpPeripherals() {
  Game &game = *game_;
  Core &core = game.core;

  watchdog_init(); // PSXPORT_WATCHDOG=<sec>: abort + backtrace if a frame
                   // stalls
  load_exe(exePath_, &core);

  gte_init();                 // GTE (COP2)
  mdec_init();                // MDEC (FMV)
  spu_init();                 // SPU
  game.spu_audio.init();      // SDL audio sink (PSXPORT_NOAUDIO to disable)
  game.gpu.gpu_native_init(); // native GPU renderer over the guest's GP0 stream
  game.cd.overridesInit();    // native CD: drive-ready + by-LBA read
  // Hardware-service HLE. RE-07 declares only the exact retail libgte projection pair, so their
  // faithful common handlers also record the authored projection. Every unrelated sync entry
  // remains zero: reaching one still runs the guest's real body and exposes the next missing fact.
  game.platform_hle.initBuiltins();
  game.pad.overridesInit(); // native controller input
  core.r[4] = 1;
  core.r[5] = 0; // a0/a1 as the BIOS leaves them
}

int TitleSession::run() {
  // The runtime must be installed BEFORE the first Core is constructed, because Game's constructor
  // snapshots it (core.cfg, renderCapabilities, the factories). That is why `runtime_` is a member
  // and this install precedes `game_`'s construction rather than following it.
  psxport_install_game(runtime_);
  game_ = std::make_unique<Game>();

  if (!selfProvision()) {
    return 1;
  }

  bringUpPeripherals();

  Core &core = game_->core;
  core.runtime->registerOverrides(*game_);
  native_boot_run(&core);
  lucent::info("boot", "native boot returned");
  // Everything this session owns — the Game (and with it the disc, the audio sink, the GPU and the
  // Lightrec executor that prints run-end telemetry), then the runtime — dies when this frame ends.
  return 0;
}

} // namespace ts2