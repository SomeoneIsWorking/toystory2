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
  // MEMORY CARD / LOAD-SAVE overlays; a separate phase so pollFrontEnd does not start a second guest call.
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

// Finite operations of main 0x8007A9E8; one stepOuterLoop call performs at most one of them.
class OuterLoopBoundary {
public:
  virtual ~OuterLoopBoundary() = default;

  virtual void initializeFrontEnd() = 0;
  virtual void restartColdFrontEnd() = 0;
  // One display field of the intro movies; true once finished.
  virtual bool stepIntroMovies() = 0;
  virtual void finishColdFrontEnd() = 0;
  virtual void prepareFrontEnd() = 0;
  // One display field of the front-end poll: nullopt while running, the event once it returns.
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
  // One display field of the overlay (0x800415E4 MEMORY CARD, 0x8004171C LOAD/SAVE), chosen by the
  // begin* call; true once the guest call has returned.
  virtual bool stepMemoryScreen() = 0;
  virtual void beginMemorySelection() = 0;
  virtual void beginLoadSaveSelection() = 0;
  virtual void restartFrontEnd() = 0;
  virtual bool residentActive() const = 0;
  virtual void updateResident() = 0;
  virtual PostResidentTransition finishResident() = 0;
  // One display field of the post-level transition: nullopt while running, the event once it returns.
  virtual std::optional<int> pollLevelTransitionEvent() = 0;
  virtual void shutdown() = 0;
};

void stepOuterLoop(OuterLoopState &state, OuterLoopBoundary &boundary);

constexpr uint32_t residentUpdateAddress(bool alternateMode) {
  return alternateMode ? 0x8007B850u : 0x8007B254u;
}

} // namespace ts2
