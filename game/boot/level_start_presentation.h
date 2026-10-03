#pragma once

#include <cstdint>

class Core;

namespace ts2 {

// The address the level start's first presentation is at: FUN_8007C278 in the exact SLUS_008.93
// bytes, called by the level start FUN_8007BEC4 as `FUN_8007C278(DAT_800A16A8, DAT_800A120C)`.
inline constexpr std::uint32_t levelStartPresentationAddress() {
  return 0x8007C278u;
}

// True when the guest's own demo guard rewrites the requested graphic to id 0, which is exactly when
// FUN_8007C278 would load and present `gfx\loading.raw`. Pure decision, no guest state.
bool demoGuardForcesLoadingCard(int demoMode);

// What the port owns at FUN_8007C278, and only that.
//
// RE-20/RE-22: the routine opens `if ((param_2 != 0) && (param_2 != 0x7b)) param_1 = 0;`, hands the
// result to the scene-graphic selector FUN_8003FB0C (whose case 0 is `gfx\loading.raw`) and presents
// it through its own 28-field fade. `param_2` is the attract/demo flag `DAT_800A120C`, so on exactly
// the attract routes the guard rewrites the requested graphic to the game's own LOADING card. Under
// instant CD that card covers no load — the load that would have covered it already completed — so it
// is not presented there. This is a complete transition, not a skipped step: the routine's own entry
// stores are still made, and the level start's next callee re-establishes the fade before it draws.
//
// EVERY OTHER ROUTE IS THE GUEST'S, INCLUDING THE PLAYER'S, AND THE PORT DOES NOT INTERCEPT IT. The
// player's level card is the level's own title graphic, presented by the routine's own 28-field fade
// inside the level start's own field-spanning call — the same call, on the same guest PC, with the
// same fields delivered. So the override is ARMED for a level start only when the guard fires, and
// RETIRED otherwise; the decision is the same `DAT_800A120C` the guest's own guard reads, taken once
// per level start by `game/frame/resident_preparation.cpp`, which is where the flag is published.
//
// The decision is a per-ROUTE decision rather than a per-call one on purpose. An override that ran
// the original as a nested call from inside the level start put the guest's own asset load and its
// interrupt-driven CD wait one dispatch deeper than the guest ever had them, and the level start
// faulted at the CD wait continuation (the BIOS trampoline 0x8008B378) immediately afterwards.
class LevelStartPresentation {
public:
  // Arm or retire the override for one level start, from the demo flag the guest's guard reads.
  void arm(Core &core, int playbackMode);

  // The armed override: the demo-forced card, suppressed. Refuses a route its own guard exempts.
  void run(Core &core);
};

// The override entry, installed by `arm` through the title's own resident registration.
void levelStartFirstPresentationEntry(Core *core);

} // namespace ts2
