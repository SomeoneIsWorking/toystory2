#include "widescreen/guest_widescreen.h"

namespace ts2 {
namespace {

const GuestWidescreenProjection kPolicy;

} // namespace

const GuestWidescreenProjection &guestWidescreenPolicy() {
  return kPolicy;
}

} // namespace ts2
