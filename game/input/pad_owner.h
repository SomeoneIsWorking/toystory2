#pragma once

#include <cstdint>

class Core;

namespace ts2 {

// Native owner for the digital pad: boot, shutdown and packet-decode boundaries that libpad would own.
class PadOwner {
public:
  void install(Core &core);

  // Publish the host packet into the retail slot buffers; this title's decoder reads them, not the SIO path.
  void service(Core &core);

  void initialize(Core &core);
  void shutdown(Core &core);

  // What the guest's own decoder answers from the published packet: active-low, release `0xFF`.
  uint16_t decode(Core &core);
};

} // namespace ts2
