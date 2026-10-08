#pragma once

#include <cstdint>

namespace ts2 {

// Operations at the resident main-loop boundary, so tests can run the shipping order without guest RAM.
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
