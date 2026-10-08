// One boot-to-exit run of Toy Story 2: owns runtime and Game, teardown by destruction.
#pragma once

#include "runtime/toystory2_runtime.h"

#include <memory>

class Game;

namespace ts2 {

// Default executable and the name SYSTEM.CNF boots directly on the disc
// (BOOT = cdrom:\SLUS_008.93;1).
inline constexpr const char *kDefaultExe = "scratch/bin/toystory2/SLUS_008.93";
inline constexpr const char *kDiscExePath = "\\SLUS_008.93";

class TitleSession final {
public:
  explicit TitleSession(const char *exePath);

  ~TitleSession();

  TitleSession(const TitleSession &) = delete;
  TitleSession &operator=(const TitleSession &) = delete;

  int run();

private:
  bool selfProvision();

  void bringUpPeripherals();

  const char *exePath_;
  // Declared BEFORE game_ so runtime_ outlives it: framework keeps a reference to runtime.
  ToyStory2Runtime runtime_;
  std::unique_ptr<Game> game_;
};

} // namespace ts2