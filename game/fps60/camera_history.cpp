#include "fps60/camera_history.h"

#include "core.h"

#include <algorithm>
#include <cmath>
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

// THE CUT BOUND IS THE GUEST'S. Camera producer 0x8002C848 publishes every authored angle as a 12-bit
// turn and, each field, compares it with the game's own target for that axis as
// `uVar8 = (target - angle) & 0xfff; if (uVar8 < 0x801) { forward } else { uVar8 - 0x1000 }` — half a
// turn is where the game itself decides a difference runs backwards rather than forwards. A camera
// the game is STEERING never crosses that within one field: its swing is a per-field rate, not a
// jump. So an authored angle that moves by half a turn or more between two consecutive fields is a
// discontinuity in the guest's own terms, and the frame after it has no halfway: it is presented as
// the real frame, exactly as the first field after a level start is.
//
// The POSITION is deliberately not part of the rule. The game publishes no per-field bound for how
// far its camera may travel, so any threshold here would be this port's number rather than the
// guest's; the level-start route the frame driver already observes covers the camera being re-authored
// with the room.
constexpr int kCameraCutRotation = kRotationHalfPeriod;

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

// The shortest signed distance between two authored angles, in the guest's own 12-bit turn units.
int rotationStep(uint16_t from, uint16_t to) {
  const int wrappedDelta = (static_cast<int>(to) - static_cast<int>(from) + kRotationHalfPeriod) & kRotationMask;
  return wrappedDelta - kRotationHalfPeriod;
}

bool isCameraCut(const ResidentCameraSample &from, const ResidentCameraSample &to) {
  for (int axis = 0; axis < 3; ++axis) {
    if (std::abs(rotationStep(from.rotation[axis], to.rotation[axis])) >= kCameraCutRotation) {
      return true;
    }
  }
  return false;
}

} // namespace

void ResidentCameraHistory::reset() {
  previous_ = {};
  current_ = {};
  ready_ = false;
  continuous_ = true;
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
    continuous_ = true;
    return;
  }
  previous_ = current_;
  current_ = sample;
  continuous_ = !isCameraCut(previous_, current_);
}

bool ResidentCameraHistory::ready() const {
  return ready_;
}

bool ResidentCameraHistory::continuous() const {
  return ready_ && continuous_;
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
