// A pad recording is keyed on guest screen state, not pad-frame count, so each word holds still while its
// screen is up. No counters: a ticking word would re-key every frame.
#pragma once

#include "input_phase.h" // psxport: psx::input::kUnkeyedPhase

#include <cstdint>

class Core;

namespace ts2 {

// Pad-replay phase; stateless, every answer is read from the Core.
class InputPhase {
public:
  // The phase for this pad frame; never psx::input::kUnkeyedPhase.
  std::uint64_t of(Core &core) const;

  // Four components, 26 bits, so the key cannot collide with kUnkeyedPhase.
  static constexpr std::uint64_t
  pack(std::uint32_t playbackMode, std::uint32_t frontEndLive, std::uint32_t selectionActive, std::uint32_t levelId) {
    return (static_cast<std::uint64_t>(playbackMode & 0xFFu) << 18) |
           (static_cast<std::uint64_t>(frontEndLive & 0x1u) << 17) |
           (static_cast<std::uint64_t>(selectionActive & 0x1u) << 16) | (levelId & 0xFFFFu);
  }
};

} // namespace ts2
