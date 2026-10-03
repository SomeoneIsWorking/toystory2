// resident_temporal_source.h — Toy Story 2's 60 fps in-between: its own primitives, interpolated.
//
// The real frame is the guest's captured frame, presented verbatim. The in-between is that same frame
// with each vertex the framework can pair to the previous real frame (psxport's
// GuestGeometrySceneSource, by projection provenance) moved halfway back toward where that vertex was.
// Camera motion and character animation both arrive this way, because both are already in the guest's
// projected vertices; nothing here re-runs or lerps the guest's camera or animation state.
//
// The title owns two things: which guest calls are producer instances (ResidentProjectionScopes) and
// whether the frame about to be presented is continuous with the previous one (this class).
#pragma once

#include "guest_geometry_scene_source.h"

namespace ts2::render {

class ResidentTemporalSource final : public psxport::temporal::GuestGeometrySceneSource {
protected:
  // A level start resets the resident camera history; until the camera has been captured twice there
  // is no previous gameplay frame to pair with, and a cut publishes a real frame because the guest's
  // camera jumped rather than moved.
  bool continuousWithPrevious(Core &core) override;
};

} // namespace ts2::render
