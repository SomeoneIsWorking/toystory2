// frame_boundary.h — the shipping frame boundaries: every guest call the frame turn makes.
//
// One class implements BOTH narrow boundaries (`ResidentFrameBoundary`, the measured per-frame
// operations, and `OuterLoopBoundary`, the finite title operations of the guest's main loop) because
// both run over the same Core and the same resumable field call. What each of them asks of the guest
// lives here; the order they are asked in lives in `frame/outer_loop.cpp` and
// `frame/resident_frame.cpp`.

#pragma once

#include "execution/guest_execution.h"
#include "frame/outer_loop.h"
#include "frame/resident_frame.h"
#include "frame/resident_preparation.h"

#include <cstddef>
#include <optional>
#include <string_view>

class Core;

namespace ts2 {

// Which guest call the one resumable field call is currently running.
enum class SelectionCall { screenLoop, queuedScreen, memoryCardOverlay, loadSaveOverlay };

// What a frame turn must carry from one step to the next: the leg of the guest's main loop being
// presented, the resident level preparation, the single resumable field call, and the intro-movie and
// overlay-screen progress. The boundary below is constructed afresh every step and holds only a
// reference to this, so every piece of state that has to outlive a step lives here.
class FrameCallState {
public:
  explicit FrameCallState(Core &core) : core_(core) {}

  // The one resumable guest call the frame turn owns, bound to the Core on first use.
  ResumableGuestCall &fieldCall();

private:
  friend class CoreFrameBoundary;
  Core &core_;
  OuterLoopState outerLoop_{};
  ResidentPreparation residentPreparation_{};
  std::optional<ResumableGuestCall> fieldCall_;
  std::size_t introMovieStep_ = 0;
  SelectionCall selectionCall_ = SelectionCall::screenLoop;
};

// The measured guest operations behind both boundaries, over one Core.
class CoreFrameBoundary final : public ResidentFrameBoundary, public OuterLoopBoundary {
public:
  CoreFrameBoundary(Core &core, FrameCallState &state) : core_(core), state_(state) {}

  // ResidentFrameBoundary: the measured per-frame order.
  int displayFieldQuota() const override;
  void beginLogicFrame(uint32_t frame) override;
  void sampleInput() override;
  void tickDisplayField() override;
  void serviceDeferredDisplay() override;
  void updateResidentGame() override;
  void advanceAudio() override;
  void present(int guestFields) override;

  // OuterLoopBoundary: the finite title operations of the guest's main loop.
  void initializeFrontEnd() override;
  void restartColdFrontEnd() override;
  bool stepIntroMovies() override;
  void finishColdFrontEnd() override;
  void prepareFrontEnd() override;
  std::optional<int> pollFrontEndEvent() override;
  void acknowledgeResidentEntry() override;
  void finishFrontEndPoll() override;
  bool playbackMode() const override;
  void setPlaybackMode(bool enabled) override;
  void selectPlaybackLevel() override;
  bool needsInteractiveSelection() const override;
  SelectionProgress stepInteractiveSelection() override;
  ResidentPreparationProgress prepareResident() override;
  void showMemoryDialog() override;
  bool stepMemoryScreen() override;
  void beginMemorySelection() override;
  void beginLoadSaveSelection() override;
  void restartFrontEnd() override;
  bool residentActive() const override;
  void updateResident() override;
  PostResidentTransition finishResident() override;
  std::optional<int> pollLevelTransitionEvent() override;
  void shutdown() override;

private:
  bool residentLeg() const;

  void callFiniteInitialization(uint32_t address, std::string_view owner, uint32_t a0);
  void callMemoryDispatcher(uint32_t a0, uint32_t a1, std::string_view owner);
  void beginOverlayCall(uint32_t address, const char *owner);
  void beginSelectionCall();
  uint32_t levelId(uint16_t selection) const;
  bool bootCountdownFinished() const;
  PostResidentTransition finishSequenceMemory();
  PostResidentTransition beginSequenceLevel(uint32_t level);
  PostResidentTransition finishSequenceScreen(uint32_t level);

  Core &core_;
  FrameCallState &state_;
  int fieldsDelivered_ = 0;
};

} // namespace ts2
