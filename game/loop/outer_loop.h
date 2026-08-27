#pragma once

#include <cstdint>

namespace ts2 {

enum class OuterLoopPhase {
  coldSetup,
  coldRestart,
  frontEndSetup,
  pollFrontEnd,
  interactiveSelection,
  residentSetup,
  resident,
  finished,
};

enum class PostResidentTransition {
  coldRestart,
  frontEndSetup,
  residentSetup,
  finished,
};

enum class ResidentPreparationProgress {
  pending,
  ready,
  finished,
};

struct OuterLoopState {
  OuterLoopPhase phase = OuterLoopPhase::coldSetup;
};

// Finite title operations extracted from main 0x8007A9E8. One call to stepOuterLoop performs at
// most one front-end poll, one interactive-selection iteration, or one resident update.
class OuterLoopBoundary {
public:
  virtual ~OuterLoopBoundary() = default;

  virtual void initializeFrontEnd() = 0;
  virtual void restartColdFrontEnd() = 0;
  virtual void prepareFrontEnd() = 0;
  virtual int pollFrontEndEvent() = 0;
  virtual void acknowledgeResidentEntry() = 0;
  virtual void finishFrontEndPoll() = 0;
  virtual bool playbackMode() const = 0;
  virtual void setPlaybackMode(bool enabled) = 0;
  virtual void selectPlaybackLevel() = 0;
  virtual bool needsInteractiveSelection() const = 0;
  virtual bool stepInteractiveSelection() = 0;
  virtual ResidentPreparationProgress prepareResident() = 0;
  virtual void showMemoryDialog() = 0;
  virtual void checkSaveSelection() = 0;
  virtual void loadSaveSelection() = 0;
  virtual void restartFrontEnd() = 0;
  virtual bool residentActive() const = 0;
  virtual void updateResident() = 0;
  virtual PostResidentTransition finishResident() = 0;
  virtual void shutdown() = 0;
};

void stepOuterLoop(OuterLoopState &state, OuterLoopBoundary &boundary);

constexpr uint32_t residentUpdateAddress(bool alternateMode) {
  return alternateMode ? 0x8007B850u : 0x8007B254u;
}

} // namespace ts2
