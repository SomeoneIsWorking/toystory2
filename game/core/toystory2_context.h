#pragma once

#include "overlay/shared_slot_image.h"
#include "render/resident_camera_history.h"
#include "render/resident_scene_history.h"

class Core;

namespace ts2 {

struct ToyStory2Context {
  SharedSlotImage sharedSlotImage;
  ResidentCameraHistory camera;
  ResidentSceneHistory scene;
};

ToyStory2Context &context(Core &core);

} // namespace ts2
