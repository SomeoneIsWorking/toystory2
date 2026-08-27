#pragma once

class Core;

namespace ts2 {

// Execute only the measured initialization prefix of guest main 0x8007A9E8 and return to the
// framework-owned product loop. The non-returning guest outer loop is deliberately not dispatched.
void initializeGuestMain(Core &core);
void finishGuestMainBoot(Core &core);

} // namespace ts2
