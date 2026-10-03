#pragma once

#include <cstdint>

class Core;

namespace ts2 {

// Native owner for the title's digital pad: the boot, shutdown and packet-decode boundaries libpad
// would own, plus the once-per-frame publish into the retail packet buffers. The host Pad produces
// the packet; these operations preserve the title-visible state in guest RAM without running libpad's
// VBlank-driven connection and actuator state machine.
class PadOwner {
public:
  // Install the pad-init, pad-shutdown and digital-pad-decode overrides for the resident image.
  void install(Core &core);

  // Publish the host packet into the retail slot buffers. Retail republishes it once per VBlank and
  // this title's decoder reads that buffer rather than the SIO slot path, so this rides the same
  // per-frame service that advances the host pad.
  void service(Core &core);

  // The two ends of the guest's own pad lifetime.
  void initialize(Core &core);
  void shutdown(Core &core);

  // What the guest's own decoder answers from the published packet: active-low, release `0xFF`.
  uint16_t decode(Core &core);
};

} // namespace ts2
