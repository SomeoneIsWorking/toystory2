#pragma once

class Core;

namespace ts2 {

// The title-local replacements for the boot-time synchronization owners: graphics initialization and
// shutdown without guest-owned timing, and the field barrier the host owns. Nothing else dispatches
// guest VBlank (`0x80039D60`), so this is where a display field is delivered instead.
class GraphicsSync {
public:
  // Install the graphics-init, resident-graphics-init, graphics-shutdown and field-barrier
  // replacements for the resident image.
  void install(Core &core);
};

} // namespace ts2
