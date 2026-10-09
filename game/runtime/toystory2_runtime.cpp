#include "runtime/toystory2_runtime.h"

#include "cd/file_transfer.h"
#include "core.h"
#include "facts/guest_facts.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "overlay/overlay_images.h"
#include "render/actor_producers.h"
#include "render/ordering_tables.h"
#include "render/slot_mesh_producers.h"
#include "runtime/toystory2_context.h"
#include "widescreen/guest_widescreen.h"
#include "widescreen/resident_widescreen.h"

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

const GuestPacketPoolWindows *ToyStory2Runtime::guestPacketPoolWindows() const {
  return &facts::kPacketPoolWindows;
}

const char *ToyStory2Runtime::discEnvVar() const {
  return facts::kDiscEnvVar;
}

const HostIdentity *ToyStory2Runtime::hostIdentity() const {
  return &kHostIdentity;
}

bool ToyStory2Runtime::guestVramIsPicture(const Game &) const {
  // The FrameDriver presents guest DrawOTag/VRAM output without dispatching guest VBlank.
  return true;
}

RenderCapabilities ToyStory2Runtime::renderCapabilities() const {
  // The picture is the guest's GP0 output replayed from the frame record; producers key what interpolates.
  return RenderCapabilities{
      .defaultPath = RenderPath::Record,
      .nativeRenderPath = false,
      .temporalInterpolation = true,
  };
}

bool ToyStory2Runtime::sealedFrameIsCut(Core &core) const {
  return context(core).frameCut.isCut();
}

const GuestWidescreenProjection *ToyStory2Runtime::guestWidescreenProjection() const {
  return &guestWidescreenPolicy();
}

std::unique_ptr<FrameDriver> ToyStory2Runtime::createFrameDriver(Game &game) {
  return ts2::createFrameDriver(game);
}

void ToyStory2Runtime::registerOverrides(Game &game) {
  // The FrameDriver owns field delivery; no host path dispatches guest VBlank 0x80039D60.
  ToyStory2Context &title = context(game.core);
  title.graphicsSync.install(game.core);
  title.overlays.installLoadObserver(game.core);
  cd::FileTransfer::install(game.core);
  title.soundBank.install(game.core);
  title.pad.install(game.core);
  OrderingTables::name(game.core);
  SlotMeshProducers::install(game.core);
  ActorIncarnations::install(game.core);
  ActorProducers::install(game.core);
  FrameCut::install(game.core);
  ResidentWidescreenCull::install(game.core);
}

void ToyStory2Runtime::bootInit(Core &core) {
  context(core).guestMainBoot.initialize(core);
}

} // namespace ts2
