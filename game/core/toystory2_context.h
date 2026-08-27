#pragma once

#include "render/resident_camera_history.h"
#include "render/resident_scene_history.h"

class Core;

namespace ts2 {

struct ToyStory2Context {
  ResidentCameraHistory camera;
  ResidentSceneHistory scene;
};

ToyStory2Context &context(Core &core);

} // namespace ts2
