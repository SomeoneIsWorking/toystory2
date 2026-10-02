#pragma once

#include "overlay/overlay_images.h"
#include "render/resident_camera_history.h"
#include "render/resident_projection_scopes.h"
#include "render/resident_scene_history.h"
#include "render/resident_widescreen.h"

class Core;

namespace ts2 {

struct ToyStory2Context {
  OverlayImages overlays;
  ResidentCameraHistory camera;
  ResidentSceneHistory scene;
  // Which guest calls are which producer instances, for the 60 fps in-between's vertex provenance.
  render::ResidentProjectionScopes projectionScopes;
  // The title-owned 16:9 widening of the resident frame canvas. It is per-Core because the plan and
  // the canvas geometry belong to the Core that published them, exactly like the histories above.
  ResidentWidescreenProjection widescreen;
  // True while a guest call suspended between display fields (the front-end poll) is running. Its
  // field barrier then waits for the host: it completes with the fields it asked for and exits the
  // executor at the field boundary so the host can present, instead of spinning through the loop.
  bool yieldAtFieldBarrier = false;
};

ToyStory2Context &context(Core &core);

} // namespace ts2
