#pragma once

#include "guest_widescreen_projection.h"

class Core;

namespace ts2 {

const GuestWidescreenProjection &guestWidescreenPolicy();

// Resident graphics mode uses a 512x240 authored projection/draw canvas around OFX=256, while the
// scanned presentation is 320x240. Latch all three facts through the shared guest-wide plan and apply
// the resulting horizontal projection centre to this Core.
GuestProjectionPlan latchResidentGuestProjection(Core &core);

} // namespace ts2
