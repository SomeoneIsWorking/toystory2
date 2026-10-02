// toystory2_input_phase.h — the input phase a Toy Story 2 pad recording is keyed on.
//
// WHY THIS TITLE HAS TO SUPPLY ONE. A `.pad` recording used to be one mask per pad frame counted
// from boot, so every press landed on the absolute frame it was captured on. Anything that moves the
// frame count — a movie that skips at a different frame, a CD read, a build that presents one more
// boot frame — shifts every press after it, and the recording then answers a screen it was never
// recorded against. On this title that failure is total rather than partial: the title screen's Start
// is edge-detected AND locked out for its first 30 fields (RE-23), so a press that arrives a few
// frames early is DISCARDED BY THE GUEST and the recording falls through to the idle attract route.
// Every pad route this project ever built ended in demo mode for exactly that reason.
//
// THE KEY, and why these four words. Each is a guest word that HOLDS STILL for as long as the screen
// that owns it, and each separates two screens the recording presses in:
//
//   0x800A120C  the attract/demo flag. The front end's own guard reads it (RE-22), the level start's
//               guard reads it, and it is 0 on every player route. A recording must never answer a
//               demo screen.
//   0x800A1670  the cold-front-end word the FMV player reads to decide whether a skip ends the whole
//               intro (RE-19). It is 0 through the four intro movies — whose pad word is not serviced
//               at all, so a press there only skips a movie — and 1 once the front end polls.
//   0x800A1420  the selection-active flag, which the MAIN MENU reads to choose between its "START
//               GAME" and "CONTINUE GAME" entries. 0 at the title and the main menu, 1 from the
//               level-select screen through the level.
//   0x800A16A8  the selected level id, published by the level start and read as its argument.
//
// NOT A COUNTER. Nothing here free-runs while a screen is up, so a screen is one segment however long
// it lasts. A word that ticks would re-key every frame and make the recording describe a moving
// target — the failure `psxport/runtime/psx/input_phase.h` warns about.
#pragma once

#include "input_phase.h" // psxport: psx::input::kUnkeyedPhase

#include <cstdint>
#include <string>

class Core;

namespace ts2 {

// The Toy Story 2 pad-replay phase. Stateless by construction — every answer is read from the Core —
// and held by value in the runtime, so no process-global state can select a different instance.
class InputPhase {
public:
  // The phase for this pad frame. A title that declares a phase must never produce
  // psx::input::kUnkeyedPhase, so the halves are masked and `of` asserts the property rather than
  // trusting it.
  std::uint64_t of(Core &core) const;

  // How a phase reads in a log line and in a replay report, for example
  // "attract=0 front-end=1 selecting=1 level=0x1".
  static std::string describe(std::uint64_t phase);

  // The packed form, exposed so a test can cross-check it against the header's addresses without
  // restating the packing. Four components, 26 bits wide, so the key can never collide with
  // psx::input::kUnkeyedPhase.
  static constexpr std::uint64_t
  pack(std::uint32_t playbackMode, std::uint32_t frontEndLive, std::uint32_t selectionActive, std::uint32_t levelId) {
    return (static_cast<std::uint64_t>(playbackMode & 0xFFu) << 18) |
           (static_cast<std::uint64_t>(frontEndLive & 0x1u) << 17) |
           (static_cast<std::uint64_t>(selectionActive & 0x1u) << 16) | (levelId & 0xFFFFu);
  }
  static constexpr std::uint32_t playbackModeOf(std::uint64_t phase) {
    return static_cast<std::uint32_t>((phase >> 18) & 0xFFu);
  }
  static constexpr std::uint32_t frontEndLiveOf(std::uint64_t phase) {
    return static_cast<std::uint32_t>((phase >> 17) & 0x1u);
  }
  static constexpr std::uint32_t selectionActiveOf(std::uint64_t phase) {
    return static_cast<std::uint32_t>((phase >> 16) & 0x1u);
  }
  static constexpr std::uint32_t levelIdOf(std::uint64_t phase) {
    return static_cast<std::uint32_t>(phase & 0xFFFFu);
  }
};

} // namespace ts2
