#pragma once

#include "overlay/level_slot_image.h"
#include "overlay/overlay_slot.h"
#include "overlay/shared_slot_image.h"

#include <array>

class Core;

namespace ts2 {

// The code-image owners of every guest-RAM slot the retail loader fills with executable modules:
// LEVEL at 0x800D12C0 and the shared MEMORY/FMV slot at 0x800D5D20. The slots do not overlap and are
// co-resident, so each keeps its own active identity.
class OverlayImages {
public:
  OverlayImages() : level_(LevelSlotImage::makeSlot()), shared_(SharedSlotImage::makeSlot()) {}

  OverlaySlot &level() {
    return level_;
  }
  OverlaySlot &shared() {
    return shared_;
  }
  // The slot a load to `destination` fills, or null when the destination is not an overlay slot.
  OverlaySlot *slotAt(std::uint32_t destination);

private:
  OverlaySlot level_;
  OverlaySlot shared_;
};

// Observe the retail file loader and authenticate/publish or retire the slot each load fills.
void installOverlayLoadObserver(Core &core);

} // namespace ts2
