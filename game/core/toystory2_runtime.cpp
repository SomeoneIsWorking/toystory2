#include "toystory2_runtime.h"

#include "boot/guest_main_boot.h"
#include "boot/native_sync_overrides.h"
#include "core.h"
#include "game.h"
#include "input/native_pad_owner.h"
#include "legacy_game_interface.h"
#include "loop/toystory2_frame_driver.h"
#include "render/guest_widescreen.h"
#include "render/resident_scene_history.h"
#include "toystory2_context.h"

namespace ts2 {

ToyStory2Runtime::ToyStory2Runtime() : LegacyGameRuntimeAdapter(legacy::measuredConfig, legacy::compatibilityHooks) {}

void *ToyStory2Runtime::createContext(Core &) {
  return new ToyStory2Context();
}

void ToyStory2Runtime::destroyContext(void *context) {
  delete static_cast<ToyStory2Context *>(context);
}

RenderCapabilities ToyStory2Runtime::renderCapabilities() const {
  return RenderCapabilities::widescreenOnly();
}

const GuestWidescreenProjection *ToyStory2Runtime::guestWidescreenProjection() const {
  return &guestWidescreenPolicy();
}

std::unique_ptr<FrameDriver> ToyStory2Runtime::createFrameDriver(Game &game) {
  return ts2::createFrameDriver(game);
}

void ToyStory2Runtime::registerOverrides(Game &game) {
  // The title FrameDriver owns field delivery directly. In particular, no graphics-init override
  // registers a host turn and no host path dispatches guest VBlank 0x80039D60.
  installNativeSyncOverrides(game.core);
  installNativePadOverrides(game.core);
  installResidentSceneObservationOverrides(game.core);
}

void ToyStory2Runtime::bootInit(Core &core) {
  initializeGuestMain(core);
}

} // namespace ts2
