#include "frame/frame_cut.h"

#include "core.h"
#include "execution_exit.h"
#include "native_dispatch.h"
#include "runtime/toystory2_context.h"

#include <lucent/log.h>

namespace ts2 {
namespace {

void cameraPlacement(Core *core) {
  psx::cpu::callOriginalToReturn(
      *core, FrameCut::kCameraPlacement, psx::cpu::ExecutionBudget::currentTurn(*core), "camera placement");
  context(*core).frameCut.noteCameraPlaced();
}

void cameraUpdate(Core *core) {
  // Only FUN_800646A8 and FUN_80074F80 write the counter, both after this update in the frame.
  const bool direct = static_cast<std::int32_t>(core->mem_r32(FrameCut::kCameraBlendFields)) < 1;
  psx::cpu::callOriginalToReturn(
      *core, FrameCut::kCameraUpdate, psx::cpu::ExecutionBudget::currentTurn(*core), "camera update");
  context(*core).frameCut.noteCameraUpdate(direct, (core->mem_r16(FrameCut::kCameraSource) & 1u) != 0u);
}

} // namespace

void FrameCut::install(Core &core) {
  psx::cpu::installNativeOverride(core, kCameraPlacement, "frame-cut-camera-placement", cameraPlacement);
  psx::cpu::installNativeOverride(core, kCameraUpdate, "frame-cut-camera-update", cameraUpdate);
}

void FrameCut::noteCameraUpdate(bool direct, bool lookCamera) {
  if (direct && lookCamera_.has_value() && *lookCamera_ != lookCamera) {
    cameraSwitched_ = true;
  }
  lookCamera_ = lookCamera;
}

void FrameCut::notePassEnded(Core &core) {
  const SceneIdentity current{
      .level = core.mem_r32(kLevelId),
      .areaObjects = core.mem_r32(kAreaObjects),
  };
  const bool sceneChanged = !previous_.has_value() || *previous_ != current;
  cut_ = sceneChanged || cameraPlaced_ || cameraSwitched_;
  lucent::debug("cut",
                "cut={} scene={} placed={} switched={} level={} area={:08X}",
                cut_,
                sceneChanged,
                cameraPlaced_,
                cameraSwitched_,
                current.level,
                current.areaObjects);
  previous_ = current;
  cameraPlaced_ = false;
  cameraSwitched_ = false;
}

} // namespace ts2
