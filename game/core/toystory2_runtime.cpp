#include "toystory2_runtime.h"

#include "boot/guest_main_boot.h"
#include "boot/native_sync_overrides.h"
#include "core.h"
#include "game.h"
#include "guest_facts.h"
#include "input/native_pad_owner.h"
#include "loop/toystory2_frame_driver.h"
#include "overlay/overlay_images.h"
#include "render/guest_widescreen.h"
#include "render/resident_scene_history.h"
#include "toystory2_context.h"

namespace ts2 {

void *ToyStory2Runtime::createContext(Core &) {
  return new ToyStory2Context();
}

void ToyStory2Runtime::destroyContext(void *context) {
  delete static_cast<ToyStory2Context *>(context);
}

const GuestProgramImage *ToyStory2Runtime::guestProgramImage() const {
  return &facts::kProgramImage;
}

const PlatformHlePlan *ToyStory2Runtime::platformHlePlan() const {
  return &facts::kPlatformHlePlan;
}

const GuestPadBufferLayout *ToyStory2Runtime::guestPadBufferLayout() const {
  return &facts::kPadBufferLayout;
}

const GuestCdStreamCallbackLayout *ToyStory2Runtime::guestCdStreamCallbackLayout() const {
  return &facts::kCdStreamCallbackLayout;
}

const char *ToyStory2Runtime::discEnvVar() const {
  return facts::kDiscEnvVar;
}

const HostIdentity *ToyStory2Runtime::hostIdentity() const {
  return &kHostIdentity;
}

bool ToyStory2Runtime::guestVramIsPicture(const Game &) const {
  // The measured FrameDriver still dispatches the resident guest renderer and presents guest
  // DrawOTag/VRAM output, including upload-only screens, without dispatching guest VBlank, so guest
  // VRAM is the picture throughout the verified route. A native producer would replace this with a
  // dynamic answer rather than a second copy of the rule.
  return true;
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
  installOverlayLoadObserver(game.core);
  installNativePadOverrides(game.core);
  installResidentSceneObservationOverrides(game.core);
  context(game.core).widescreen.install(game.core);
}

void ToyStory2Runtime::bootInit(Core &core) {
  initializeGuestMain(core);
}

} // namespace ts2
