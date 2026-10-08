#pragma once

class Core;

namespace ts2 {

// Guest main 0x8007A9E8 synchronous prefix and MEMORY overlay init.
class GuestMainBoot {
public:
  void initialize(Core &core);

  // Runs in a host frame after field quota delivery, since it may use the field barrier.
  void finishOverlayInitialization(Core &core);
};

} // namespace ts2
