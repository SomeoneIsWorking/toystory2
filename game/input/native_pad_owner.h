#pragma once

#include <cstdint>

class Core;

namespace ts2 {

// Native owner for the title's digital-pad boot, shutdown, and packet decode boundaries. The host
// Pad producer writes the retail packet buffer; these operations preserve the title-visible state
// without running libpad's VBlank-driven connection/actuator state machine.
void initializeNativePad(Core &core);
void shutdownNativePad(Core &core);
uint16_t decodeNativeDigitalPad(Core &core);
void installNativePadOverrides();

} // namespace ts2
