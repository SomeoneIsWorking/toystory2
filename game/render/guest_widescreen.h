#pragma once

#include "guest_widescreen_projection.h"

class Core;

namespace ts2 {

// The title's own answer to "is this picture wider than the console's 4:3?". The widening itself is
// owned by render/resident_widescreen.h, which is where the measured resident canvas lives.
const GuestWidescreenProjection &guestWidescreenPolicy();

} // namespace ts2
