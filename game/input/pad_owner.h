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
void installNativePadOverrides(Core &core);

// Retail republishes the pad packet into the guest slot buffer (0x800CF8A0) once per VBlank, and this
// title's decoder reads that buffer rather than the SIO slot path. Before this ran per frame the buffer
// held only what boot wrote.
void serviceNativePad(Core &core);

} // namespace ts2
