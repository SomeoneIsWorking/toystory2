#include "render/guest_widescreen.h"

#include "core.h"
#include "game.h"
#include "gpu_vk.h"
#include "mods.h"
#include "proj_params.h"

#include <cstdlib>

namespace ts2 {
namespace {

constexpr int kResidentProjectionWidth = 512;
constexpr int kResidentProjectionHeight = 240;
constexpr int kResidentProjectionCenterY = 120;

class ToyStory2GuestWidescreen final : public GuestWidescreenProjection {
public:
  PresentationAspect presentationAspect(const Core &core) const override {
    if (core.game == nullptr) {
      return PresentationAspect::Standard4x3;
    }
    switch (core.game->mods.aspect) {
    case ASPECT_4_3:
      return PresentationAspect::Standard4x3;
    case ASPECT_16_9:
      return PresentationAspect::Wide16x9;
    case ASPECT_21_9:
      return PresentationAspect::UltraWide21x9;
    case ASPECT_AUTO:
      return PresentationAspect::MatchSink;
    default:
      std::abort();
    }
  }
};

} // namespace

const GuestWidescreenProjection &guestWidescreenPolicy() {
  static const ToyStory2GuestWidescreen policy;
  return policy;
}

GuestProjectionPlan latchResidentGuestProjection(Core &core) {
  const GuestProjectionPlan plan = gpu_vk_latch_guest_projection(
      &core, {.extent = {kResidentProjectionWidth, kResidentProjectionHeight}, .drawWidth = kResidentProjectionWidth});
  libgte_set_geom_offset(&core, plan.projectionCenterX, kResidentProjectionCenterY);
  return plan;
}

} // namespace ts2
