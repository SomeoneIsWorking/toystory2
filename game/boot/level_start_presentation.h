#pragma once

#include <cstdint>

class Core;

namespace ts2 {

// The address the level start's first presentation is at: FUN_8007C278 in the exact SLUS_008.93
// bytes, called by the level start FUN_8007BEC4 as `FUN_8007C278(DAT_800A16A8, DAT_800A120C)`.
inline constexpr std::uint32_t levelStartPresentationAddress() {
  return 0x8007C278u;
}

// Armed per route rather than per call: running the original as a nested call put the guest's CD wait
// one dispatch deeper than the guest ever had it.
class LevelStartPresentation {
public:
  static bool demoGuardForcesLoadingCard(int demoMode);

  void arm(Core &core, int playbackMode);

  // Refuses a route its own guard exempts.
  void run(Core &core);

private:
  // Resolves the (image identity, address) key that makes `arm` idempotent.
  static bool firstPresentationInstalled(Core &core);
};

void levelStartFirstPresentationEntry(Core *core);

} // namespace ts2
