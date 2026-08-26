// main.cpp — the Toy Story 2 port's process entry point.
//
// Installs ToyStory2Runtime + RecompRegistry, brings up
// the framework's PSX hardware backends, loads the retail executable, and
// enters the native boot. After the install nothing here names anything but
// framework symbols.
//
// RE-02 supplies the generated registry and this is the shipping launcher. Game behavior remains
// guest code; the host runtime restores only the measured field boundary, and current boot enters
// emitted FMV before stopping at the honest shared BIOS A0:0x25 boundary.
#include "cfg.h"
#include "core.h"
#include "disc.h"
#include "fs_util.h"
#include "game.h"
#include "toystory2_runtime.h"
#include <stdio.h>

extern "C" {
void watchdog_init(void);
void mdec_init(void);
void spu_init(void);
}

void load_exe(const char *path, Core *c); // runtime/recomp/boot.cpp (framework)
void native_boot_run(Core *c);            // runtime/recomp/native_boot.cpp (framework)
void gte_init(void);
int selftest_run(const char *path); // runtime/recomp/selftest.cpp (framework harness)

extern void ts2_install_recomp(); // game/core/recomp_register.cpp

// The retail US executable, as it is named on the disc. SYSTEM.CNF boots it
// directly
// (`BOOT = cdrom:\SLUS_008.93;1` — measured 2026-08-12), so there is no boot
// stub LoadExec'ing a second image the way Tomba!2's SCUS_944.54 -> MAIN.EXE
// hand-off does; the framework's stub stage is unused.
//
// THE ENGINE IS NOT ALL IN THIS FILE, WHICH IS THIS PORT'S DEFINING STRUCTURAL
// FACT: the original 21 plain overlays hold 29.1% of the measured code-bearing
// bytes, and FMV/FMV.BIN is a 22nd entered code module (C014). RE-03 proves
// their two physical slots; RE-02 emits the resident executable and all 22
// modules. Do not read "load the boot exe and go" as "the boot exe is the game".
static const char *kDefaultExe = "scratch/bin/toystory2/SLUS_008.93";
static const char *kDiscExePath = "\\SLUS_008.93";

int main(int argc, char **argv) {
  // Process-lifetime derived owner. Installation must precede the first Core, which snapshots it.
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  ts2_install_recomp();

  const char *path = argc > 1 ? argv[1] : kDefaultExe;

  Game *game = new Game();
  Core *c = &game->core;

  // Self-provision the executable so the binary is runnable straight from a
  // disc image with no prior step (disc resolution: $PSXPORT_TS2_DISC, .env, or
  // a *.chd in the working directory — the same order tools/resolve_disc.py
  // implements host-side).
  if (!Fs::exists(path)) {
    cfg_logw("boot", "%s missing — extracting from disc", path);
    if (!disc_extract_file(&game->disc, kDiscExePath, path)) {
      cfg_loge("boot",
               "extraction failed: provide a disc (PSXPORT_TS2_DISC, .env, or "
               "a *.chd in "
               "the working directory), or run `python3 tools/extract_exe.py`");
      return 1;
    }
  }

  // PSXPORT_SELFTEST=<name>: run the framework's headless selftest harness
  // instead of booting.
  {
    const char *st = cfg_str("PSXPORT_SELFTEST");
    if (st && *st) {
      return selftest_run(path);
    }
  }

  watchdog_init(); // PSXPORT_WATCHDOG=<sec>: abort + backtrace if a frame
                   // stalls
  load_exe(path, c);

  gte_init();                  // GTE (COP2)
  mdec_init();                 // MDEC (FMV)
  spu_init();                  // SPU
  game->spu_audio.init();      // SDL audio sink (PSXPORT_NOAUDIO to disable)
  game->gpu.gpu_native_init(); // native GPU renderer over the guest's GP0 stream
  game->cd.overridesInit();    // native CD: drive-ready + by-LBA read
  // Hardware-service HLE. RE-07 currently declares only the exact retail libgte projection pair, so
  // their faithful common handlers also record the authored projection. Every unrelated sync entry
  // remains zero: reaching one still runs the guest's real body and exposes the next missing fact.
  game->platform_hle.initBuiltins();
  game->pad.overridesInit(); // native controller input
  c->r[4] = 1;
  c->r[5] = 0; // a0/a1 as the BIOS leaves them

  c->runtime->registerOverrides(*game);
  native_boot_run(c);
  cfg_logi("boot", "native boot returned");
  return 0;
}
