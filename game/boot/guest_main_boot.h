#pragma once

class Core;

namespace ts2 {

// The measured initialization prefix of guest main `0x8007A9E8`, and the overlay initialization that
// follows it. The guest's own non-returning outer loop is deliberately not dispatched: from the
// second call on, `ts2::stepOuterLoop` owns the turn.
class GuestMainBoot {
public:
  // Run the synchronous prefix: the libc init, the measured boot words, and the graphics initializer
  // as a finite initialization transaction.
  void initialize(Core &core);

  // Load and initialize the MEMORY overlay and publish the display mode. It may use the measured
  // field barrier, so it runs inside a host frame, after the shell has delivered a field quota.
  void finishOverlayInitialization(Core &core);
};

} // namespace ts2
