#pragma once

class Core;

namespace ts2 {

// Installs title-local replacements for the two boot-time synchronization owners. Generated
// functions remain present and callable as supers; only the generated registry's runtime route is
// changed.
void initializeResidentGraphicsWithoutGuestVSync(Core &core);
void installNativeSyncOverrides();

} // namespace ts2
