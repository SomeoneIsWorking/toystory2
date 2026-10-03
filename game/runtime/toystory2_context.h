#pragma once

#include "boot/level_start_presentation.h"
#include "fps60/camera_history.h"
#include "fps60/projection_scopes.h"
#include "overlay/overlay_images.h"
#include "render/scene_history.h"
#include "widescreen/resident_widescreen.h"

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
  // The level start's first presentation (FUN_8007C278): the demo-forced LOADING card is suppressed
  // there, and every other route runs the guest's own field-spanning routine. It holds the resume
  // point of that run, so it is per-Core like the histories above.
  LevelStartPresentation levelStartPresentation;
  // True while a guest call suspended between display fields (the front-end poll) is running. Its
  // field barrier then waits for the host: it completes with the fields it asked for and exits the
  // executor at the field boundary so the host can present, instead of spinning through the loop.
  bool yieldAtFieldBarrier = false;
};

ToyStory2Context &context(Core &core);

} // namespace ts2
