// The shipping frame boundaries: every guest call the frame turn makes.

#pragma once

#include "execution/guest_execution.h"
#include "frame/field_call.h"
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

// State that outlives a step; the boundary is rebuilt every step and only references it.
class FrameCallState {
public:
  explicit FrameCallState(Core &core) : core_(core) {}

  FieldCall &fieldCall();

private:
  friend class CoreFrameBoundary;
  Core &core_;
  OuterLoopState outerLoop_{};
  ResidentPreparation residentPreparation_{};
  FieldCall fieldCall_;
  std::size_t introMovieStep_ = 0;
  SelectionCall selectionCall_ = SelectionCall::screenLoop;
};

class CoreFrameBoundary final : public ResidentFrameBoundary, public OuterLoopBoundary {
public:
  CoreFrameBoundary(Core &core, FrameCallState &state) : core_(core), state_(state) {}

  int displayFieldQuota() const override;
  void beginLogicFrame(uint32_t frame) override;
  void sampleInput() override;
  void tickDisplayField() override;
  void serviceDeferredDisplay() override;
  void updateResidentGame() override;
  void advanceAudio() override;
  void present(int guestFields) override;

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
