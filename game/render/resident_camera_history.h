#pragma once

#include <cstdint>

class Core;

namespace ts2 {

// Authored camera input consumed by producer 0x8002C848 before resident scene root 0x8002A070.
// Other update calls intervene, but the producer's published camera feeds that later scene pass.
// Positions are the title's signed integer units. Rotations are one turn in 4096 units.
struct ResidentCameraSample {
  int32_t position[3] = {};
  uint16_t rotation[3] = {};
};

struct InterpolatedResidentCamera {
  float position[3] = {};
  float rotation[3] = {};
};

class ResidentCameraHistory {
public:
  void reset();
  void capture(Core &core);
  void capture(const ResidentCameraSample &sample);

  bool ready() const;
  const ResidentCameraSample &previous() const;
  const ResidentCameraSample &current() const;
  InterpolatedResidentCamera interpolate(float t) const;

private:
  ResidentCameraSample previous_{};
  ResidentCameraSample current_{};
  bool ready_ = false;
};

} // namespace ts2
