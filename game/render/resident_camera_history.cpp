#include "render/resident_camera_history.h"

#include "core.h"

#include <algorithm>
#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

constexpr uint32_t kResidentCamera = 0x800C1540u;
constexpr uint32_t kPositionOffset = 0u;
constexpr uint32_t kRotationOffset = 12u;
constexpr uint16_t kRotationMask = 0x0FFFu;
constexpr int kRotationPeriod = 0x1000;
constexpr int kRotationHalfPeriod = kRotationPeriod / 2;

ResidentCameraSample readCamera(Core &core) {
  ResidentCameraSample sample;
  for (uint32_t axis = 0; axis < 3; ++axis) {
    sample.position[axis] = static_cast<int32_t>(core.mem_r32(kResidentCamera + kPositionOffset + axis * 4));
    sample.rotation[axis] = core.mem_r16(kResidentCamera + kRotationOffset + axis * 2) & kRotationMask;
  }
  return sample;
}

float interpolateRotation(uint16_t from, uint16_t to, float t) {
  const int wrappedDelta = (static_cast<int>(to) - static_cast<int>(from) + kRotationHalfPeriod) & kRotationMask;
  const int shortestDelta = wrappedDelta - kRotationHalfPeriod;
  return static_cast<float>(from) + static_cast<float>(shortestDelta) * t;
}

} // namespace

void ResidentCameraHistory::reset() {
  previous_ = {};
  current_ = {};
  ready_ = false;
}

void ResidentCameraHistory::capture(Core &core) {
  const ResidentCameraSample sample = readCamera(core);
  capture(sample);
  lucent::debug("ts2-camera",
                "authored camera pos=({},{},{}) rot=({},{},{})",
                sample.position[0],
                sample.position[1],
                sample.position[2],
                sample.rotation[0],
                sample.rotation[1],
                sample.rotation[2]);
}

void ResidentCameraHistory::capture(const ResidentCameraSample &sample) {
  if (!ready_) {
    previous_ = sample;
    current_ = sample;
    ready_ = true;
    return;
  }
  previous_ = current_;
  current_ = sample;
}

bool ResidentCameraHistory::ready() const {
  return ready_;
}

const ResidentCameraSample &ResidentCameraHistory::previous() const {
  if (!ready_) {
    std::abort();
  }
  return previous_;
}

const ResidentCameraSample &ResidentCameraHistory::current() const {
  if (!ready_) {
    std::abort();
  }
  return current_;
}

InterpolatedResidentCamera ResidentCameraHistory::interpolate(float t) const {
  if (!ready_) {
    std::abort();
  }
  const float clamped = std::clamp(t, 0.0F, 1.0F);
  InterpolatedResidentCamera result;
  for (int axis = 0; axis < 3; ++axis) {
    const float from = static_cast<float>(previous_.position[axis]);
    result.position[axis] = from + (static_cast<float>(current_.position[axis]) - from) * clamped;
    result.rotation[axis] = interpolateRotation(previous_.rotation[axis], current_.rotation[axis], clamped);
  }
  return result;
}

} // namespace ts2
