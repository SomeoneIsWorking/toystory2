#include "loop/resident_frame.h"

namespace ts2 {

void stepResidentFrame(ResidentFrameBoundary &boundary, uint32_t frame) {
  const int guestFields = boundary.displayFieldQuota();
  boundary.beginLogicFrame(frame);
  boundary.sampleInput();

  for (int field = 0; field < guestFields; ++field) {
    boundary.tickDisplayField();
  }
  boundary.serviceDeferredDisplay();

  boundary.updateResidentGame();
  boundary.advanceAudio();
  boundary.present(guestFields);
}

} // namespace ts2
