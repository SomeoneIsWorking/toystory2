#pragma once

#include "overlay/level_slot_image.h"
#include "overlay/overlay_slot.h"
#include "overlay/shared_slot_image.h"

#include <array>

class Core;

namespace ts2 {

// Code-image owners of the guest-RAM slots the loader fills with modules: LEVEL at 0x800D12C0 and the
// shared MEMORY/FMV slot at 0x800D5D20. The slots are co-resident, each with its own active identity.
class OverlayImages {
public:
  OverlayImages() : level_(LevelSlotImage::makeSlot()), shared_(SharedSlotImage::makeSlot()) {}

  OverlaySlot &level() {
    return level_;
  }
  OverlaySlot &shared() {
    return shared_;
  }
  OverlaySlot *slotAt(std::uint32_t destination);

  void installLoadObserver(Core &core);

private:
  OverlaySlot level_;
  OverlaySlot shared_;
};

} // namespace ts2
