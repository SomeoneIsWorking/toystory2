#pragma once

class Core;

namespace ts2 {

// True when FUN_8007C278's own demo guard rewrites the requested graphic to id 0, which is exactly
// when the level start would load and present `gfx\loading.raw`. This is the whole decision the
// level-start override makes, and it is pure: no guest state, no I/O, no clock.
bool demoGuardForcesLoadingCard(int demoMode);

// Installs title-local replacements for the two boot-time synchronization owners. Generated
// functions remain present and callable as supers; only the generated registry's runtime route is
// changed.
void initializeResidentGraphicsWithoutGuestVSync(Core &core);
void installNativeSyncOverrides(Core &core);

} // namespace ts2
