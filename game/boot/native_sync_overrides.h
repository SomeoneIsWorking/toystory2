#pragma once

class Core;

namespace ts2 {

// Installs the title-local replacements for the boot-time synchronization owners: graphics
// initialization and shutdown without guest-owned timing, and the field barrier the host owns.
void initializeResidentGraphicsWithoutGuestVSync(Core &core);
void installNativeSyncOverrides(Core &core);

} // namespace ts2
