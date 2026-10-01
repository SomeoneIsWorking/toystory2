#include "render/guest_widescreen.h"

#include "core.h"
#include "game.h"
#include "mods.h"

#include <cstdlib>

namespace ts2 {
namespace {

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

} // namespace ts2
