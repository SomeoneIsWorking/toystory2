#include "runtime/toystory2_runtime.h"

#include "cd/file_transfer.h"
#include "core.h"
#include "facts/guest_facts.h"
#include "fps60.h"
#include "fps60/projection_scopes.h"
#include "fps60/temporal_source.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "legacy_game_hooks.h"
#include "overlay/overlay_images.h"
#include "render/scene_history.h"
#include "render/view_matrix.h"
#include "runtime/toystory2_context.h"
#include "widescreen/guest_widescreen.h"

namespace ts2 {
namespace {

// The only legacy hook this runtime binds. The framework reaches the game's own camera through
// `Core::hooks`, and a direct runtime otherwise supplies none at all — `fps60ReadSceneCam` would then
// have nothing to call and `Fps60::sceneCam` refuses rather than guessing a camera. Everything else
// in the table stays null, which each consumer already treats as "this game does not have one".
const GameHooks kResidentFps60Hooks = {
    .fps60ReadSceneCam =
        +[](Core *core, float view[3][3], float translation[3]) {
          render::readResidentView(*core, view, translation);
        },
};

} // namespace

ToyStory2Runtime::ToyStory2Runtime() {
  bindLegacyInterface(nullptr, &kResidentFps60Hooks);
}

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
  // Gte stays the shipping path and its real frames are presented exactly as captured. The temporal
  // product is an in-between made of the guest's own primitives, each vertex interpolated between two
  // real frames by projection provenance (fps60/temporal_source.h, docs/issues/0036).
  return RenderCapabilities::guestInterpolated();
}

std::unique_ptr<psx::frame::TemporalFramePresentation> ToyStory2Runtime::createTemporalFramePresentation(Game &game) {
  return std::make_unique<Fps60>(game, std::make_unique<render::ResidentTemporalSource>());
}

const GuestWidescreenProjection *ToyStory2Runtime::guestWidescreenProjection() const {
  return &guestWidescreenPolicy();
}

std::unique_ptr<FrameDriver> ToyStory2Runtime::createFrameDriver(Game &game) {
  return ts2::createFrameDriver(game);
}

void ToyStory2Runtime::registerOverrides(Game &game) {
  // The 60fps camera seam is bound in this runtime's constructor, because the framework reads it from
  // `Core::hooks` and takes it from the runtime installed before the first Core is built.
  // The title FrameDriver owns field delivery directly. In particular, no graphics-init override
  // registers a host turn and no host path dispatches guest VBlank 0x80039D60.
  ToyStory2Context &title = context(game.core);
  title.graphicsSync.install(game.core);
  title.overlays.installLoadObserver(game.core);
  cd::FileTransfer::install(game.core);
  title.soundBank.install(game.core);
  title.pad.install(game.core);
  title.scene.install(game.core);
  title.projectionScopes.install(game.core);
  title.widescreen.install(game.core);
}

void ToyStory2Runtime::bootInit(Core &core) {
  context(core).guestMainBoot.initialize(core);
}

} // namespace ts2
