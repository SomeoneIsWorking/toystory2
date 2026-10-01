// title_session.h — ONE boot-to-exit run of Toy Story 2, as an owner with a destructor.
//
// main() used to do the boot inline and keep the result alive forever: `new Game()` was never
// deleted, and the title runtime was a function-local `static`. So `~Game` — the disc hunk-cache
// report, the memory-card syscall log, and the Lightrec executor's run-end telemetry — never ran at
// the ordinary end of a run. This class owns both objects as members, performs the boot sequence in
// `run()`, and tears everything down by destruction.
#pragma once

#include "toystory2_runtime.h"

#include <memory>

class Game;

namespace ts2 {

// The default executable, and the name SYSTEM.CNF boots directly on the disc
// (`BOOT = cdrom:\SLUS_008.93;1` — measured 2026-08-12), so there is no boot stub LoadExec'ing a
// second image the way Tomba!2's SCUS_944.54 -> MAIN.EXE hand-off does; the framework's stub stage
// is unused.
//
// THE ENGINE IS NOT ALL IN THE BOOT EXECUTABLE, WHICH IS THIS PORT'S DEFINING STRUCTURAL FACT: the
// original 21 plain overlays hold 29.1% of the measured code-bearing bytes, and FMV/FMV.BIN is a
// 22nd entered code module (C014). RE-03 proves their two physical slots. Do not read "load the boot
// exe and go" as "the boot exe is the game".
inline constexpr const char *kDefaultExe = "scratch/bin/toystory2/SLUS_008.93";
inline constexpr const char *kDiscExePath = "\\SLUS_008.93";

class TitleSession final {
public:
  // `exePath` is either `kDefaultExe` or a path the operator passed. It is not touched here; the
  // session only reads it, so a caller may build the session on a stack and let it go out of scope.
  explicit TitleSession(const char *exePath);

  // Defined in the translation unit that has the framework's complete `Game`, so this header never
  // needs it and every destruction path releases the Game exactly once.
  ~TitleSession();

  TitleSession(const TitleSession &) = delete;
  TitleSession &operator=(const TitleSession &) = delete;

  // Boot, run the guest, and tear down on return. The return value is the process exit code.
  int run();

private:
  // Extract the executable from the disc when it is not already on the host, so the binary is
  // runnable straight from a disc image with no prior step. Returns false when there is nothing to
  // extract from.
  bool selfProvision();

  // Bring up every backend this Game owns, in the order the framework binds them.
  void bringUpPeripherals();

  const char *exePath_;
  // Declared BEFORE `game_`, so it is destroyed AFTER it: the framework's installed runtime pointer
  // (psxport_install_game takes a reference and keeps it) must outlive every Game that read it.
  ToyStory2Runtime runtime_;
  std::unique_ptr<Game> game_;
};

} // namespace ts2