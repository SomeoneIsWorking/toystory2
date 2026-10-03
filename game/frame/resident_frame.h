#pragma once

#include <cstdint>

namespace ts2 {

// The finite operations at Toy Story 2's resident main-loop boundary. Keeping the measured ordering
// behind this narrow interface lets a hermetic test exercise the shipping sequence without copying
// it or constructing guest RAM, audio, or a renderer.
class ResidentFrameBoundary {
public:
  virtual ~ResidentFrameBoundary() = default;

  virtual int displayFieldQuota() const = 0;
  virtual void beginLogicFrame(uint32_t frame) = 0;
  virtual void sampleInput() = 0;
  virtual void tickDisplayField() = 0;
  virtual void serviceDeferredDisplay() = 0;
  virtual void updateResidentGame() = 0;
  virtual void advanceAudio() = 0;
  virtual void present(int guestFields) = 0;
};

void stepResidentFrame(ResidentFrameBoundary &boundary, uint32_t frame);

} // namespace ts2
