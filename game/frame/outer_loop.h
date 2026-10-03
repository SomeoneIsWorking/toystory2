#pragma once

#include <cstdint>
#include <optional>

namespace ts2 {

enum class OuterLoopPhase {
  coldSetup,
  coldRestart,
  introMovies,
  frontEndSetup,
  pollFrontEnd,
  // The MEMORY CARD / LOAD-SAVE modal overlay screens. These are guest calls that span display
  // fields, so the loop needs a phase of its own to keep stepping them: re-entering pollFrontEnd
  // would start a SECOND guest call while the first is still running.
  memoryScreen,
  interactiveSelection,
  residentSetup,
  resident,
  levelTransition,
  finished,
};

enum class PostResidentTransition {
  coldRestart,
  frontEndSetup,
  residentSetup,
  levelTransition,
  finished,
};

// How one interactive-selection step ended: still running, a level chosen (go prepare it), or the
// player backed out of the screen (re-enter the front-end poll).
enum class SelectionProgress {
  pending,
  chosen,
  backToFrontEnd,
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
// most one front-end poll field, one interactive-selection iteration, one resident update, or one
// post-level transition field.
class OuterLoopBoundary {
public:
  virtual ~OuterLoopBoundary() = default;

  virtual void initializeFrontEnd() = 0;
  virtual void restartColdFrontEnd() = 0;
  // Advance the blocking intro-movie sequence by one display field; true once it has finished.
  virtual bool stepIntroMovies() = 0;
  virtual void finishColdFrontEnd() = 0;
  virtual void prepareFrontEnd() = 0;
  // One display field of the front-end poll: nullopt while it is still running, the event once it returns.
  virtual std::optional<int> pollFrontEndEvent() = 0;
  virtual void acknowledgeResidentEntry() = 0;
  virtual void finishFrontEndPoll() = 0;
  virtual bool playbackMode() const = 0;
  virtual void setPlaybackMode(bool enabled) = 0;
  virtual void selectPlaybackLevel() = 0;
  virtual bool needsInteractiveSelection() const = 0;
  virtual SelectionProgress stepInteractiveSelection() = 0;
  virtual ResidentPreparationProgress prepareResident() = 0;
  virtual void showMemoryDialog() = 0;
  // One display field of the MEMORY CARD selection (0x800415E4). false while that guest call is
  // still running, true once it has returned and its save-selection answer is published.
  // One display field of the modal overlay screen (0x800415E4 MEMORY CARD, 0x8004171C LOAD/SAVE):
  // false while that guest call is still running, true once it has returned. Which of the two is
  // running is chosen by `beginMemorySelection` / `beginLoadSaveSelection` before the first step.
  virtual bool stepMemoryScreen() = 0;
  virtual void beginMemorySelection() = 0;
  virtual void beginLoadSaveSelection() = 0;
  virtual void restartFrontEnd() = 0;
  virtual bool residentActive() const = 0;
  virtual void updateResident() = 0;
  virtual PostResidentTransition finishResident() = 0;
  // One display field of the post-level transition (the guest's next asset set load and the
  // transition screen it draws): nullopt while it is still running, the event once it returns.
  virtual std::optional<int> pollLevelTransitionEvent() = 0;
  virtual void shutdown() = 0;
};

void stepOuterLoop(OuterLoopState &state, OuterLoopBoundary &boundary);

constexpr uint32_t residentUpdateAddress(bool alternateMode) {
  return alternateMode ? 0x8007B850u : 0x8007B254u;
}

} // namespace ts2
