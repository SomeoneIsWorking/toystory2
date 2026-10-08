#include "frame/resident_frame.h"

namespace ts2 {

void stepResidentFrame(ResidentFrameBoundary &boundary, uint32_t frame) {
  const int guestFields = boundary.displayFieldQuota();
  boundary.beginLogicFrame(frame);
  boundary.sampleInput();

  for (int field = 0; field < guestFields; ++field) {
    boundary.tickDisplayField();
  }
  boundary.serviceDeferredDisplay();
  // The deferred display drew the OT the previous update built; seal it before this update writes the next.
  boundary.present(guestFields);

  boundary.updateResidentGame();
  boundary.advanceAudio();
}

} // namespace ts2
